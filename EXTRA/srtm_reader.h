/*
  SRTMReader - Written By Benjamin Jack Cullen.

  Reads compressed (.hgt.gz) and uncompressed (.hgt) SRTM elevation tiles.

  SRTM tile conventions:
    - File names encode the SW corner: N51E000.hgt, S34W071.hgt.gz, etc.
    - Samples are big-endian int16, arranged N→S then W→E.
    - Void / ocean cells are stored as -32768.
    - SRTM1: 3601×3601 grid, 1 arc-second (~30 m) resolution.
    - SRTM3: 1201×1201 grid, 3 arc-second (~90 m) resolution.

  Memory:
    Tile data is allocated from PSRAM (SPIRAM) when available, otherwise from
    the main heap.  SRTM3 tiles require ≈2.9 MB; SRTM1 tiles require ≈25 MB.
    Ensure sufficient PSRAM is configured before loading SRTM1 tiles.

  Decompression:
    .hgt.gz files are decompressed using tinfl (miniz.h) which is
    provided by the esp_rom component and available on all ESP-IDF 5.x targets.
*/

#ifndef SRTM_READER_H
#define SRTM_READER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// Elevation value used for void cells and out-of-range queries.
#define SRTM_VOID_ELEVATION   ((int16_t)-32768)

// Grid dimensions for each resolution.
#define SRTM1_SAMPLES         3601    // 1 arc-second, ~30 m
#define SRTM3_SAMPLES         1201    // 3 arc-second, ~90 m

// Maximum supported uncompressed tile size (SRTM1, bytes).
#define SRTM_MAX_TILE_BYTES   ((size_t)SRTM1_SAMPLES * SRTM1_SAMPLES * 2u)

// Streaming input buffer used during gzip decompression (bytes).
#ifndef SRTM_GZ_IN_BUF_SIZE
#define SRTM_GZ_IN_BUF_SIZE   4096
#endif

typedef enum {
    SRTM_RES_UNKNOWN = 0,
    SRTM_RES_SRTM1   = 1,   // 1 arc-second
    SRTM_RES_SRTM3   = 3,   // 3 arc-second
} srtm_res_t;

struct SRTMTileInfo {
    int        lat;      // SW-corner latitude  (-90 … 89)
    int        lon;      // SW-corner longitude (-180 … 179)
    srtm_res_t res;
    int        samples;  // grid side length (3601 or 1201)
};

class SRTMReader {
public:
    SRTMReader();
    ~SRTMReader();

    /**
     * Load an SRTM tile from an .hgt or .hgt.gz file.
     *
     * The file path must be accessible via the ESP-IDF VFS (e.g. mounted SD
     * card at /sdcard).  The lat/lon of the tile is derived from the filename;
     * the resolution is inferred from the decompressed data size.
     *
     * Calling loadTile() while a tile is already loaded first calls
     * unloadTile() automatically.
     *
     * @param filepath  Absolute VFS path, e.g. "/sdcard/srtm/N51E000.hgt.gz"
     * @return true on success.
     */
    bool loadTile(const char* filepath);

    /**
     * Release the tile buffer and reset state.
     */
    void unloadTile();

    /** @return true if a tile is currently loaded. */
    bool isTileLoaded() const { return _loaded; }

    /**
     * Return the elevation (metres, WGS84 geoid) at the given coordinate.
     *
     * Returns SRTM_VOID_ELEVATION when:
     *   - no tile is loaded,
     *   - the coordinate falls outside the loaded tile, or
     *   - the nearest sample is a void cell (-32768).
     *
     * @param lat         WGS84 latitude  (-90 … +90).
     * @param lon         WGS84 longitude (-180 … +180).
     * @param interpolate When true, bilinear interpolation is applied using
     *                    the four surrounding samples.  Void neighbours are
     *                    ignored; if all four are void the result is void.
     */
    int16_t getElevation(double lat, double lon,
                         bool   interpolate = false) const;

    /** @return Tile metadata (valid only when isTileLoaded() is true). */
    const SRTMTileInfo& getTileInfo() const { return _info; }

    /**
     * Parse a standard SRTM filename and return the SW-corner lat/lon.
     *
     * Accepts bare names ("N51E000") or full paths with optional extensions
     * (".hgt", ".hgt.gz").  Both upper and lower case hemisphere letters are
     * accepted.
     *
     * @param path  File name or path to parse.
     * @param lat   Output SW-corner latitude.
     * @param lon   Output SW-corner longitude.
     * @return true on success.
     */
    static bool parseTileFilename(const char* path, int* lat, int* lon);

private:
    int16_t*     _data;
    SRTMTileInfo _info;
    bool         _loaded;

    // Resolve resolution from total decompressed byte count.
    static srtm_res_t _resFromBytes(size_t bytes);

    // Load raw (uncompressed) .hgt data; filesize must be SRTM1 or SRTM3 size.
    bool _loadRaw(FILE* f, long filesize);

    // Decompress a gzip stream starting at the current file position.
    // Allocates _data internally.
    bool _loadGzip(FILE* f);

    // Byte-swap all loaded samples from big-endian to native byte order.
    void _byteSwapAll();

    // Return the raw (native byte order) sample at grid position (row, col).
    // Row 0 is the northernmost row.  Returns SRTM_VOID_ELEVATION on OOB.
    int16_t _sample(int row, int col) const;
};

#endif // SRTM_READER_H
