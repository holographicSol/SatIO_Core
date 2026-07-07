/*
  SRTMReader - Written By Benjamin Jack Cullen.
*/

#include "srtm_reader.h"

#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "miniz.h"       // tinfl_decompressor, tinfl_decompress – esp_rom component

static const char* TAG = "SRTMReader";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static inline int16_t _swap16(int16_t v) {
    return (int16_t)(((uint16_t)v >> 8u) | ((uint16_t)v << 8u));
}

// Allocate from PSRAM when available, fall back to main heap.
static void* _psram_alloc(size_t size) {
    void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = malloc(size);
    return p;
}

// ---------------------------------------------------------------------------
// Skip a gzip header at the current file position.
// Returns false if the magic bytes are wrong or a read error occurs.
// After a successful return the file position is at the first compressed byte.
// ---------------------------------------------------------------------------
static bool _skipGzipHeader(FILE* f) {
    uint8_t hdr[10];
    if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) return false;
    if (hdr[0] != 0x1fu || hdr[1] != 0x8bu) {
        ESP_LOGE(TAG, "Not a gzip file (bad magic)");
        return false;
    }
    if (hdr[2] != 8u) {
        ESP_LOGE(TAG, "Unsupported gzip compression method %u", hdr[2]);
        return false;
    }

    const uint8_t flg = hdr[3];

    // FEXTRA – skip extra field
    if (flg & 0x04u) {
        uint8_t xlen_buf[2];
        if (fread(xlen_buf, 1, 2, f) != 2) return false;
        uint16_t xlen = (uint16_t)xlen_buf[0] | ((uint16_t)xlen_buf[1] << 8u);
        if (fseek(f, xlen, SEEK_CUR) != 0) return false;
    }

    // FNAME – skip null-terminated original file name
    if (flg & 0x08u) {
        int c;
        while ((c = fgetc(f)) != EOF && c != 0) { /* skip */ }
        if (c == EOF) return false;
    }

    // FCOMMENT – skip null-terminated comment
    if (flg & 0x10u) {
        int c;
        while ((c = fgetc(f)) != EOF && c != 0) { /* skip */ }
        if (c == EOF) return false;
    }

    // FHCRC – skip 2-byte header CRC
    if (flg & 0x02u) {
        if (fseek(f, 2, SEEK_CUR) != 0) return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Streaming tinfl decompression from FILE* into a pre-allocated linear buffer.
// The file position must be at the start of the raw deflate stream (i.e.
// after the gzip header has already been consumed).
//
// Returns the number of bytes written to out_buf, or 0 on error.
// ---------------------------------------------------------------------------
static size_t _tinflFromFile(FILE* f, uint8_t* out_buf, size_t out_buf_size) {
    tinfl_decompressor decomp;
    tinfl_init(&decomp);

    uint8_t in_buf[SRTM_GZ_IN_BUF_SIZE];
    size_t  out_pos = 0;

    for (;;) {
        size_t in_count = fread(in_buf, 1u, sizeof(in_buf), f);
        bool   last_input = feof(f) || (in_count < sizeof(in_buf));

        const uint8_t* p_in       = in_buf;
        size_t         remaining  = in_count;

        do {
            size_t in_bytes  = remaining;
            size_t out_bytes = out_buf_size - out_pos;

            mz_uint32 flags = TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
            if (!last_input) flags |= TINFL_FLAG_HAS_MORE_INPUT;

            tinfl_status st = tinfl_decompress(
                &decomp,
                p_in,  &in_bytes,
                out_buf, out_buf + out_pos, &out_bytes,
                flags);

            p_in      += in_bytes;
            remaining -= in_bytes;
            out_pos   += out_bytes;

            if (st == TINFL_STATUS_DONE) {
                return out_pos;                         // success
            }
            if ((int)st < (int)TINFL_STATUS_DONE) {
                ESP_LOGE(TAG, "tinfl error %d", (int)st);
                return 0;                               // decompression error
            }
            if (out_pos >= out_buf_size) {
                ESP_LOGE(TAG, "Output buffer too small");
                return 0;
            }
        } while (remaining > 0);

        if (last_input) {
            // Compressed stream did not signal TINFL_STATUS_DONE before EOF.
            ESP_LOGE(TAG, "Unexpected end of gzip stream");
            return 0;
        }
    }
}

// ---------------------------------------------------------------------------
// SRTMReader
// ---------------------------------------------------------------------------

SRTMReader::SRTMReader() : _data(nullptr), _loaded(false) {
    memset(&_info, 0, sizeof(_info));
}

SRTMReader::~SRTMReader() {
    unloadTile();
}

void SRTMReader::unloadTile() {
    if (_data) {
        free(_data);
        _data = nullptr;
    }
    memset(&_info, 0, sizeof(_info));
    _loaded = false;
}

// ---------------------------------------------------------------------------

srtm_res_t SRTMReader::_resFromBytes(size_t bytes) {
    if (bytes == (size_t)SRTM1_SAMPLES * SRTM1_SAMPLES * 2u) return SRTM_RES_SRTM1;
    if (bytes == (size_t)SRTM3_SAMPLES * SRTM3_SAMPLES * 2u) return SRTM_RES_SRTM3;
    return SRTM_RES_UNKNOWN;
}

void SRTMReader::_byteSwapAll() {
    const size_t total = (size_t)_info.samples * _info.samples;
    for (size_t i = 0; i < total; ++i) {
        _data[i] = _swap16(_data[i]);
    }
}

int16_t SRTMReader::_sample(int row, int col) const {
    if (row < 0 || row >= _info.samples || col < 0 || col >= _info.samples) {
        return SRTM_VOID_ELEVATION;
    }
    return _data[(size_t)row * _info.samples + col];
}

// ---------------------------------------------------------------------------

bool SRTMReader::_loadRaw(FILE* f, long filesize) {
    srtm_res_t res = _resFromBytes((size_t)filesize);
    if (res == SRTM_RES_UNKNOWN) {
        ESP_LOGE(TAG, "Unrecognised file size %ld for raw .hgt", filesize);
        return false;
    }

    _data = (int16_t*)_psram_alloc((size_t)filesize);
    if (!_data) {
        ESP_LOGE(TAG, "Failed to allocate %ld bytes for tile", filesize);
        return false;
    }

    fseek(f, 0, SEEK_SET);
    size_t n = fread(_data, 1u, (size_t)filesize, f);
    if (n != (size_t)filesize) {
        ESP_LOGE(TAG, "Short read: expected %ld, got %zu", filesize, n);
        free(_data);
        _data = nullptr;
        return false;
    }

    _info.res     = res;
    _info.samples = (res == SRTM_RES_SRTM1) ? SRTM1_SAMPLES : SRTM3_SAMPLES;
    _byteSwapAll();
    return true;
}

// ---------------------------------------------------------------------------

bool SRTMReader::_loadGzip(FILE* f) {
    if (!_skipGzipHeader(f)) return false;

    // Allocate worst-case output buffer (SRTM1 size).
    const size_t max_out = SRTM_MAX_TILE_BYTES;
    uint8_t* buf = (uint8_t*)_psram_alloc(max_out);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes for decompression", max_out);
        return false;
    }

    size_t written = _tinflFromFile(f, buf, max_out);
    if (written == 0) {
        free(buf);
        return false;
    }

    srtm_res_t res = _resFromBytes(written);
    if (res == SRTM_RES_UNKNOWN) {
        ESP_LOGE(TAG, "Decompressed %zu bytes: not a valid SRTM tile size", written);
        free(buf);
        return false;
    }

    _data         = (int16_t*)buf;
    _info.res     = res;
    _info.samples = (res == SRTM_RES_SRTM1) ? SRTM1_SAMPLES : SRTM3_SAMPLES;
    _byteSwapAll();
    return true;
}

// ---------------------------------------------------------------------------

bool SRTMReader::loadTile(const char* filepath) {
    if (_loaded) unloadTile();

    // Parse lat/lon from the filename.
    if (!parseTileFilename(filepath, &_info.lat, &_info.lon)) {
        ESP_LOGE(TAG, "Cannot parse tile coordinates from \"%s\"", filepath);
        return false;
    }

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open \"%s\"", filepath);
        return false;
    }

    // Determine whether the file is gzip-compressed by reading the magic bytes.
    uint8_t magic[2] = {0, 0};
    fread(magic, 1u, 2u, f);
    rewind(f);

    bool ok = false;
    if (magic[0] == 0x1fu && magic[1] == 0x8bu) {
        // gzip
        ok = _loadGzip(f);
    } else {
        // raw .hgt – measure file size
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        ok = _loadRaw(f, sz);
    }

    fclose(f);

    if (ok) {
        _loaded = true;
        ESP_LOGI(TAG, "Loaded %s tile (%d×%d) for %c%02d%c%03d",
                 (_info.res == SRTM_RES_SRTM1) ? "SRTM1" : "SRTM3",
                 _info.samples, _info.samples,
                 _info.lat  >= 0 ? 'N' : 'S', abs(_info.lat),
                 _info.lon  >= 0 ? 'E' : 'W', abs(_info.lon));
    }
    return ok;
}

// ---------------------------------------------------------------------------

int16_t SRTMReader::getElevation(double lat, double lon, bool interpolate) const {
    if (!_loaded) return SRTM_VOID_ELEVATION;

    // Bounds check against this 1°×1° tile.
    if (lat  <  _info.lat       || lat  > _info.lat  + 1.0 ||
        lon  <  _info.lon       || lon  > _info.lon  + 1.0) {
        return SRTM_VOID_ELEVATION;
    }

    const int N = _info.samples;

    // Map lat/lon to fractional grid position.
    // Row 0 = north edge (lat + 1°), row N-1 = south edge (lat).
    double row_f = ((double)(_info.lat + 1) - lat)  * (N - 1);
    double col_f = (lon - (double)_info.lon)         * (N - 1);

    if (!interpolate) {
        int row = (int)(row_f + 0.5);
        int col = (int)(col_f + 0.5);
        // Clamp to valid range.
        if (row < 0) row = 0; else if (row >= N) row = N - 1;
        if (col < 0) col = 0; else if (col >= N) col = N - 1;
        return _sample(row, col);
    }

    // Bilinear interpolation.
    int r0 = (int)floor(row_f);
    int c0 = (int)floor(col_f);
    double tr = row_f - r0;   // fractional row offset  (0 … 1)
    double tc = col_f - c0;   // fractional column offset (0 … 1)

    int16_t s00 = _sample(r0,     c0    );
    int16_t s01 = _sample(r0,     c0 + 1);
    int16_t s10 = _sample(r0 + 1, c0    );
    int16_t s11 = _sample(r0 + 1, c0 + 1);

    // Count valid (non-void) neighbours.
    int valid = ((s00 != SRTM_VOID_ELEVATION) ? 1 : 0) +
                ((s01 != SRTM_VOID_ELEVATION) ? 1 : 0) +
                ((s10 != SRTM_VOID_ELEVATION) ? 1 : 0) +
                ((s11 != SRTM_VOID_ELEVATION) ? 1 : 0);
    if (valid == 0) return SRTM_VOID_ELEVATION;

    // Fall back to nearest-neighbour when any corner is void.
    if (valid < 4) {
        int row_n = (int)(row_f + 0.5);
        int col_n = (int)(col_f + 0.5);
        if (row_n < 0) row_n = 0; else if (row_n >= N) row_n = N - 1;
        if (col_n < 0) col_n = 0; else if (col_n >= N) col_n = N - 1;
        int16_t nn = _sample(row_n, col_n);
        return (nn != SRTM_VOID_ELEVATION) ? nn : SRTM_VOID_ELEVATION;
    }

    double v = (1.0 - tr) * ((1.0 - tc) * s00 + tc * s01)
             +        tr  * ((1.0 - tc) * s10 + tc * s11);
    return (int16_t)round(v);
}

// ---------------------------------------------------------------------------

bool SRTMReader::parseTileFilename(const char* path, int* lat, int* lon) {
    if (!path || !lat || !lon) return false;

    // Locate the basename (last '/' or '\\' component).
    const char* base = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }

    // Expect at least [N/S]DD[E/W]DDD = 7 characters.
    if (strlen(base) < 7) return false;

    // Hemisphere letters (case-insensitive).
    char ns = (char)toupper((unsigned char)base[0]);
    if (ns != 'N' && ns != 'S') return false;

    if (!isdigit((unsigned char)base[1]) || !isdigit((unsigned char)base[2])) return false;
    int lat_val = (base[1] - '0') * 10 + (base[2] - '0');

    char ew = (char)toupper((unsigned char)base[3]);
    if (ew != 'E' && ew != 'W') return false;

    if (!isdigit((unsigned char)base[4]) ||
        !isdigit((unsigned char)base[5]) ||
        !isdigit((unsigned char)base[6])) return false;
    int lon_val = (base[4] - '0') * 100 + (base[5] - '0') * 10 + (base[6] - '0');

    *lat = (ns == 'S') ? -lat_val : lat_val;
    *lon = (ew == 'W') ? -lon_val : lon_val;
    return true;
}
