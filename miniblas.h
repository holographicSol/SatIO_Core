/** @file miniblas.h
 * KFCore
 * @author Jan Zwiener (jan@zwiener.org)
 *
 * @brief Minimal generic BLAS implementation
 *
 * Note: all matrices are stored in column-major order.
 * 
 BSD 3-Clause License

Copyright (c) 2024, Jan Zwiener

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

4. Attribution Requirement:

   Redistributions in any form must include the following acknowledgment:

   'This product includes KFCore, developed by Jan Zwiener.'

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **/

/******************************************************************************
 * SYSTEM INCLUDE FILES
 ******************************************************************************/

/******************************************************************************
 * PROJECT INCLUDE FILES
 ******************************************************************************/

/******************************************************************************
 * DEFINES
 ******************************************************************************/

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

/******************************************************************************
 * LOCAL DATA DEFINITIONS
 ******************************************************************************/

/******************************************************************************
 * LOCAL FUNCTION PROTOTYPES
 ******************************************************************************/

/******************************************************************************
 * FUNCTION PROTOTYPES
 ******************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

    int lsame_(const char* a, const char* b);

    int strsm_(const char* side, const char* uplo, const char* transa, const char* diag, int* m,
              int* n, float* alpha, const float* a, int* lda, float* b, int* ldb);

    int sgemm_(const char* transa, const char* transb, int* m, int* n, int* k, float* alpha,
              float* a, int* lda, float* b, int* ldb, float* beta, float* c, int* ldc);

    int ssyrk_(const char* uplo, const char* trans, int* n, int* k, float* alpha, float* a,
              int* lda, float* beta, float* c, int* ldc);

    int ssymm_(const char* side, const char* uplo, int* m, int* n, float* alpha, float* a,
              int* lda, float* b, int* ldb, float* beta, float* c, int* ldc);

    int strmm_(const char* side, const char* uplo, const char* transa, const char* diag, int* m,
               int* n, float* alpha, float* a, int* lda, float* b, int* ldb);

#ifdef __cplusplus
}
#endif

/* @} */
