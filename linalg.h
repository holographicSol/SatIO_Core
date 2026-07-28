/** @file linalg.h
 * KFCore
 * @author Jan Zwiener (jan@zwiener.org)
 *
 * @brief Embedded linear algebra math library
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

#define MAT_ELEM(M, row, col, numrows, numcols) ((M)[(row) + (col) * (numrows)])
#define SQRTF(x) sqrtf(x)

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

    /*** @brief matrix multiply C = alpha*A*B + beta*C
     * BLAS: ?gemm
     *
     * @param[in] ta Supply "T" (transpose A) or "N" (don't transpose A)
     * @param[in] tb Supply "T" (transpose B) or "N" (don't transpose B)
     * @param[in] n Dimension n (rows of A) (dimension after transpose)
     * @param[in] k Dimension k (cols of B) (dimension after transpose)
     * @param[in] m Dimension m (rows of B) (dimension after transpose)
     * @param[in] alpha Factor alpha
     * @param[in] A Input matrix A (n x m)
     * @param[in] B Input matrix B (m x k)
     * @param[in] beta Factor beta
     * @param[in/out] C Output matrix (n x k)
     */
    void matmul(const char* ta, const char* tb, int n, int k, int m, float alpha, const float* A,
                const float* B, float beta, float* C);

    /** @brief Multiplication of symmetric matrix A with B:
     *         C = A*B
     *  @param[in] A_sym (n x n) matrix, only upper triangular part is referenced.
     *  @param[in] B (n x m) matrix
     *  @param[in] n Rows/columns of A, rows of B
     *  @param[in] m columns of B and C
     *  @param[out] C (n x m) matrix output of the product A*B
     */
    void matmulsym(const float* A_sym, const float* B, int n, int m, float* C);

    /** @brief Fill array with an identity matrix.
     * @param[out] A To be filled (n x n).
     * @param[in] n Dimension of A. */
    void mateye(float* A, int n);

    /** @brief Calculate the lower triangular matrix L, so
     * that L*L' = A. Operation count: n^3/6 with n square roots.
     * BLAS equivalent: ?potrf
     *
     * Optmized for
     *
     * @param[in,out] A Symmetric, positive definite (n x n) matrix. Only upper
     *                  triangular part needs to be given.
     *                  Lower triangular part is overwritten with L.
     * @param[in] n Dimension of A
     * @param[in] onlyWriteLowerPart If set to 0, overwrite the upper
     *                               triangular part with zeros.
     *                               Set to e.g. -1 to leave the upper
     *                               triangular part untouched.
     * @return 0 if successful, -1 if matrix is not positive definite.
     */
    int cholesky(float* A, const int n, int onlyWriteLowerPart);

    /**
     * @brief Triangular solve
     * BLAS: ?trsm
     *
     * Solve matrix equation: A*X = B
     * @param[in]     A Given lower triangular matrix (dimension n x n)
     * @param[in,out] B Matrix being overwritten by X (dimension n x m)
     * @param[in]     n Matrix dimension (rows / columns of A)
     * @param[in]     m Matrix dimension (cols of B)
     * @param[in]     tp Transpose L?
     */
    void trisolve(const float* A, float* B, int n, int m, const char* tp);

    /**
     * @brief Triangular solve (right hand side).
     * BLAS: ?trsm
     *
     * Solve matrix equation: X*L = A
     * @param[in]     L Given lower triangular matrix (dimension n x n)
     * @param[in,out] A Matrix being overwritten by X (dimension m x n)
     * @param[in]     n Matrix dimension (rows / columns of L)
     * @param[in]     m Matrix dimension (rows of A)
     * @param[in]     tp Transpose L?
     */
    void trisolveright(const float* L, float* A, int n, int m, const char* tp);

    /** @brief Symmetric rank update. P = P - E*E'
     * @param[in,out] P Matrix (n x n) to be updated (only upper part is referenced and updated)
     * @param[in] E Matrix (n x m) including the update
     * @param[in] n Number of rows and cols in P, rows in E
     * @param[in] m Number of cols in E */
    void symmetricrankupdate(float* P, const float* E, int n, int m);

    /**
     *  @brief UDU decomposition of a symmetrical n x n matrix so that A = U*D*U'.
     *
     *    D is returned as diagonal vector d so that diag(d) = D.
     *    U Is an unit upper triangular matrix U.
     *
     * @param[in] A (n x n) input matrix
     * @param[out] U (n x n) Output upper unit triangular matrix U
     * @param[out] d (n x 1) Output vector d (D = diag(d))
     * @param[in] n Matrix dimension n
     *
     */
    int udu(const float* A, float* U, float* d, const int n);

#ifdef __cplusplus
}
#endif

/* @} */
