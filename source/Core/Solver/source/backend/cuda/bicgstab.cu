#include <cublas_v2.h>
#include <cuda_runtime.h>
#include <cusparse.h>

#include <RHI/cuda.hpp>
#include <RHI/internal/cuda_extension.hpp>
#include <RZSolver/Solver.hpp>
#include <iostream>

RUZINO_NAMESPACE_OPEN_SCOPE
namespace Solver {

// 在 namespace 级别定义静态函数
namespace {
    SolverResult performCleanBiCGStabImpl(
        cublasHandle_t cublasHandle,
        cusparseHandle_t cusparseHandle,
        const SolverConfig& config,
        int n,
        cusparseSpMatDescr_t matA_desc,
        Ruzino::cuda::CUDALinearBufferHandle dBuffer,
        Ruzino::cuda::CUDALinearBufferHandle d_b,
        Ruzino::cuda::CUDALinearBufferHandle d_x,
        Ruzino::cuda::CUDALinearBufferHandle d_r,
        Ruzino::cuda::CUDALinearBufferHandle d_r0,
        Ruzino::cuda::CUDALinearBufferHandle d_p,
        Ruzino::cuda::CUDALinearBufferHandle d_v,
        Ruzino::cuda::CUDALinearBufferHandle d_s,
        Ruzino::cuda::CUDALinearBufferHandle d_t,
        cusparseDnVecDescr_t vecX_desc,
        cusparseDnVecDescr_t vecP_desc,
        cusparseDnVecDescr_t vecV_desc,
        cusparseDnVecDescr_t vecS_desc,
        cusparseDnVecDescr_t vecT_desc)
    {
        SolverResult result;
        const float one = 1.0f, zero = 0.0f, minus_one = -1.0f;

        // Compute ||b||
        float b_norm;
        cublasSdot(
            cublasHandle,
            n,
            reinterpret_cast<float*>(d_b->get_device_ptr()),
            1,
            reinterpret_cast<float*>(d_b->get_device_ptr()),
            1,
            &b_norm);
        b_norm = sqrt(b_norm);

        if (b_norm == 0.0f) {
            result.converged = true;
            result.iterations = 0;
            result.final_residual = 0.0f;
            return result;
        }

        // r = b - A*x
        cublasScopy(
            cublasHandle,
            n,
            reinterpret_cast<float*>(d_b->get_device_ptr()),
            1,
            reinterpret_cast<float*>(d_r->get_device_ptr()),
            1);

        cusparseSpMV(
            cusparseHandle,
            CUSPARSE_OPERATION_NON_TRANSPOSE,
            &one,
            matA_desc,
            vecX_desc,
            &zero,
            vecT_desc,
            CUDA_R_32F,
            CUSPARSE_SPMV_ALG_DEFAULT,
            (void*)dBuffer->get_device_ptr());

        cublasSaxpy(
            cublasHandle,
            n,
            &minus_one,
            reinterpret_cast<float*>(d_t->get_device_ptr()),
            1,
            reinterpret_cast<float*>(d_r->get_device_ptr()),
            1);

        // Compute initial residual norm
        float r_norm_init;
        cublasSdot(
            cublasHandle,
            n,
            reinterpret_cast<float*>(d_r->get_device_ptr()),
            1,
            reinterpret_cast<float*>(d_r->get_device_ptr()),
            1,
            &r_norm_init);
        r_norm_init = sqrt(r_norm_init);

        // r0 = r (standard choice for SPD) or r0 = random for ill-conditioned
        // For better stability on ill-conditioned SPD matrices, use r0 = r
        cublasScopy(
            cublasHandle,
            n,
            reinterpret_cast<float*>(d_r->get_device_ptr()),
            1,
            reinterpret_cast<float*>(d_r0->get_device_ptr()),
            1);

        // p = r
        cublasScopy(
            cublasHandle,
            n,
            reinterpret_cast<float*>(d_r->get_device_ptr()),
            1,
            reinterpret_cast<float*>(d_p->get_device_ptr()),
            1);

        float rho_old = 1.0f, alpha = 1.0f, omega = 1.0f;
        int consecutive_small_rho = 0;  // Track breakdown tendency

        // BiCGSTAB iterations - clean implementation with breakdown recovery
        for (int iter = 0; iter < config.max_iterations; ++iter) {
            // rho = r0^T * r
            float rho;
            cublasSdot(
                cublasHandle,
                n,
                reinterpret_cast<float*>(d_r0->get_device_ptr()),
                1,
                reinterpret_cast<float*>(d_r->get_device_ptr()),
                1,
                &rho);
            
            // Adaptive breakdown detection
            float rho_threshold = 1e-30f;
            if (r_norm_init > 1e-10f) {
                rho_threshold = std::max(rho_threshold, 1e-20f * r_norm_init * r_norm_init);
            }
            
            if (abs(rho) < rho_threshold) {
                consecutive_small_rho++;
                if (consecutive_small_rho > 3 || iter < 10) {
                    // Early or repeated breakdown - cannot recover
                    result.error_message = "BiCGSTAB breakdown: rho too small";
                    break;
                }
                // Try to recover by using current residual as new r0
                cublasScopy(
                    cublasHandle,
                    n,
                    reinterpret_cast<float*>(d_r->get_device_ptr()),
                    1,
                    reinterpret_cast<float*>(d_r0->get_device_ptr()),
                    1);
                continue;
            }
            consecutive_small_rho = 0;  // Reset counter

            if (iter > 0) {
                float beta = (rho / rho_old) * (alpha / omega);

                // p = r + beta * (p - omega * v)
                float neg_omega = -omega;
                cublasSaxpy(
                    cublasHandle,
                    n,
                    &neg_omega,
                    reinterpret_cast<float*>(d_v->get_device_ptr()),
                    1,
                    reinterpret_cast<float*>(d_p->get_device_ptr()),
                    1);
                cublasSscal(
                    cublasHandle,
                    n,
                    &beta,
                    reinterpret_cast<float*>(d_p->get_device_ptr()),
                    1);
                cublasSaxpy(
                    cublasHandle,
                    n,
                    &one,
                    reinterpret_cast<float*>(d_r->get_device_ptr()),
                    1,
                    reinterpret_cast<float*>(d_p->get_device_ptr()),
                    1);
            }

            // v = A * p
            cusparseSpMV(
                cusparseHandle,
                CUSPARSE_OPERATION_NON_TRANSPOSE,
                &one,
                matA_desc,
                vecP_desc,
                &zero,
                vecV_desc,
                CUDA_R_32F,
                CUSPARSE_SPMV_ALG_DEFAULT,
                (void*)dBuffer->get_device_ptr());

            // alpha = rho / (r0^T * v)
            float r0_dot_v;
            cublasSdot(
                cublasHandle,
                n,
                reinterpret_cast<float*>(d_r0->get_device_ptr()),
                1,
                reinterpret_cast<float*>(d_v->get_device_ptr()),
                1,
                &r0_dot_v);

            // More lenient breakdown check
            if (abs(r0_dot_v) < 1e-30f || (abs(r0_dot_v) < 1e-15f * b_norm * b_norm && iter > 10)) {
                result.error_message = "BiCGSTAB breakdown: r0^T * v too small";
                break;
            }

            alpha = rho / r0_dot_v;

            // s = r - alpha * v
            cublasScopy(
                cublasHandle,
                n,
                reinterpret_cast<float*>(d_r->get_device_ptr()),
                1,
                reinterpret_cast<float*>(d_s->get_device_ptr()),
                1);
            float neg_alpha = -alpha;
            cublasSaxpy(
                cublasHandle,
                n,
                &neg_alpha,
                reinterpret_cast<float*>(d_v->get_device_ptr()),
                1,
                reinterpret_cast<float*>(d_s->get_device_ptr()),
                1);

            // Check early convergence
            float s_norm;
            cublasSdot(
                cublasHandle,
                n,
                reinterpret_cast<float*>(d_s->get_device_ptr()),
                1,
                reinterpret_cast<float*>(d_s->get_device_ptr()),
                1,
                &s_norm);
            s_norm = sqrt(s_norm);

            if (s_norm / b_norm < config.tolerance) {
                // x = x + alpha * p
                cublasSaxpy(
                    cublasHandle,
                    n,
                    &alpha,
                    reinterpret_cast<float*>(d_p->get_device_ptr()),
                    1,
                    reinterpret_cast<float*>(d_x->get_device_ptr()),
                    1);

                result.converged = true;
                result.iterations = iter + 1;
                result.final_residual = s_norm / b_norm;
                break;
            }

            // t = A * s
            cusparseSpMV(
                cusparseHandle,
                CUSPARSE_OPERATION_NON_TRANSPOSE,
                &one,
                matA_desc,
                vecS_desc,
                &zero,
                vecT_desc,
                CUDA_R_32F,
                CUSPARSE_SPMV_ALG_DEFAULT,
                (void*)dBuffer->get_device_ptr());

            // omega = (t^T * s) / (t^T * t)
            float t_dot_s, t_dot_t;
            cublasSdot(
                cublasHandle,
                n,
                reinterpret_cast<float*>(d_t->get_device_ptr()),
                1,
                reinterpret_cast<float*>(d_s->get_device_ptr()),
                1,
                &t_dot_s);
            cublasSdot(
                cublasHandle,
                n,
                reinterpret_cast<float*>(d_t->get_device_ptr()),
                1,
                reinterpret_cast<float*>(d_t->get_device_ptr()),
                1,
                &t_dot_t);
            if (t_dot_t < 1e-30f || (t_dot_t < 1e-20f * b_norm * b_norm && iter > 10)) {
                // Instead of breaking down, try to recover using a different
                // approach Use the intermediate solution x = x + alpha * p
                cublasSaxpy(
                    cublasHandle,
                    n,
                    &alpha,
                    reinterpret_cast<float*>(d_p->get_device_ptr()),
                    1,
                    reinterpret_cast<float*>(d_x->get_device_ptr()),
                    1);

                // Check if this gives acceptable solution
                float s_relative_residual = s_norm / b_norm;
                if (s_relative_residual < config.tolerance * 10) {
                    result.converged = true;
                    result.iterations = iter + 1;
                    result.final_residual = s_relative_residual;
                    break;
                }
                else {
                    result.error_message =
                        "BiCGSTAB breakdown: t^T * t too small";
                    break;
                }
            }

            omega = t_dot_s / t_dot_t;

            // x = x + alpha * p + omega * s
            cublasSaxpy(
                cublasHandle,
                n,
                &alpha,
                reinterpret_cast<float*>(d_p->get_device_ptr()),
                1,
                reinterpret_cast<float*>(d_x->get_device_ptr()),
                1);
            cublasSaxpy(
                cublasHandle,
                n,
                &omega,
                reinterpret_cast<float*>(d_s->get_device_ptr()),
                1,
                reinterpret_cast<float*>(d_x->get_device_ptr()),
                1);

            // r = s - omega * t
            cublasScopy(
                cublasHandle,
                n,
                reinterpret_cast<float*>(d_s->get_device_ptr()),
                1,
                reinterpret_cast<float*>(d_r->get_device_ptr()),
                1);
            float neg_omega2 = -omega;
            cublasSaxpy(
                cublasHandle,
                n,
                &neg_omega2,
                reinterpret_cast<float*>(d_t->get_device_ptr()),
                1,
                reinterpret_cast<float*>(d_r->get_device_ptr()),
                1);

            // Check convergence
            float r_norm;
            cublasSdot(
                cublasHandle,
                n,
                reinterpret_cast<float*>(d_r->get_device_ptr()),
                1,
                reinterpret_cast<float*>(d_r->get_device_ptr()),
                1,
                &r_norm);
            r_norm = sqrt(r_norm);

            float relative_residual = r_norm / b_norm;
            if (relative_residual < config.tolerance) {
                result.converged = true;
                result.iterations = iter + 1;
                result.final_residual = relative_residual;
                if (config.verbose) {
                    std::cout << "BiCGSTAB converged in " << iter + 1
                              << " iterations, residual: " << relative_residual
                              << std::endl;
                }
                break;
            }

            if (abs(omega) < 1e-12f) {
                result.error_message = "BiCGSTAB breakdown: omega too small";
                break;
            }

            rho_old = rho;
            result.iterations = iter + 1;
        }

        if (!result.converged && result.error_message.empty()) {
            result.error_message = "Maximum iterations reached";
            // Compute final residual
            float r_norm;
            cublasSdot(
                cublasHandle,
                n,
                reinterpret_cast<float*>(d_r->get_device_ptr()),
                1,
                reinterpret_cast<float*>(d_r->get_device_ptr()),
                1,
                &r_norm);
            result.final_residual = sqrt(r_norm) / b_norm;
        }

        return result;
    }
}  // namespace

class CudaBiCGStabSolver : public LinearSolver {
   private:
    cusparseHandle_t cusparseHandle;
    cublasHandle_t cublasHandle;
    bool initialized = false;

   public:
    CudaBiCGStabSolver()
    {
        if (cusparseCreate(&cusparseHandle) != CUSPARSE_STATUS_SUCCESS ||
            cublasCreate(&cublasHandle) != CUBLAS_STATUS_SUCCESS) {
            throw std::runtime_error("Failed to initialize CUDA libraries");
        }
        initialized = true;
    }

    ~CudaBiCGStabSolver()
    {
        if (initialized) {
            cusparseDestroy(cusparseHandle);
            cublasDestroy(cublasHandle);
        }
    }

    std::string getName() const override
    {
        return "CUDA BiCGSTAB";
    }

    bool isIterative() const override
    {
        return true;
    }
    bool requiresGPU() const override
    {
        return true;
    }

    // GPU-only interface implementation
    SolverResult solveGPU(
        int n,
        int nnz,
        const int* d_row_offsets,
        const int* d_col_indices,
        const float* d_values,
        const float* d_b,
        float* d_x,
        const SolverConfig& config = SolverConfig{}) override
    {
        printf("BiCGSTAB solveGPU called: n=%d, nnz=%d\n", n, nnz);
        auto start_time = std::chrono::high_resolution_clock::now();
        SolverResult result;

        try {
            printf("Creating matrix descriptor...\n");
            // Create cuSPARSE matrix descriptor from existing GPU data
            cusparseSpMatDescr_t matA_desc;
            cusparseCreateCsr(
                &matA_desc,
                n, n, nnz,
                (void*)d_row_offsets,
                (void*)d_col_indices,
                (void*)d_values,
                CUSPARSE_INDEX_32I,
                CUSPARSE_INDEX_32I,
                CUSPARSE_INDEX_BASE_ZERO,
                CUDA_R_32F);

            // Allocate temporary vectors
            Ruzino::cuda::CUDALinearBufferDesc vec_desc;
            vec_desc.element_count = n;
            vec_desc.element_size = sizeof(float);
            
            auto d_r = Ruzino::cuda::create_cuda_linear_buffer(vec_desc);
            auto d_r0 = Ruzino::cuda::create_cuda_linear_buffer(vec_desc);
            auto d_p_buf = Ruzino::cuda::create_cuda_linear_buffer(vec_desc);
            auto d_v = Ruzino::cuda::create_cuda_linear_buffer(vec_desc);
            auto d_s = Ruzino::cuda::create_cuda_linear_buffer(vec_desc);
            auto d_t = Ruzino::cuda::create_cuda_linear_buffer(vec_desc);

            // Create vector descriptors
            cusparseDnVecDescr_t vecX, vecB, vecP, vecV, vecS, vecT;
            cusparseCreateDnVec(&vecX, n, (void*)d_x, CUDA_R_32F);
            cusparseCreateDnVec(&vecB, n, (void*)d_b, CUDA_R_32F);
            cusparseCreateDnVec(&vecP, n, (void*)d_p_buf->get_device_ptr(), CUDA_R_32F);
            cusparseCreateDnVec(&vecV, n, (void*)d_v->get_device_ptr(), CUDA_R_32F);
            cusparseCreateDnVec(&vecS, n, (void*)d_s->get_device_ptr(), CUDA_R_32F);
            cusparseCreateDnVec(&vecT, n, (void*)d_t->get_device_ptr(), CUDA_R_32F);

            // Query SpMV buffer size
            size_t bufferSize = 0;
            const float one = 1.0f, zero = 0.0f;
            cusparseSpMV_bufferSize(
                cusparseHandle,
                CUSPARSE_OPERATION_NON_TRANSPOSE,
                (const void*)&one,
                matA_desc,
                vecP,
                (const void*)&zero,
                vecV,
                CUDA_R_32F,
                CUSPARSE_SPMV_ALG_DEFAULT,
                &bufferSize);

            Ruzino::cuda::CUDALinearBufferDesc buffer_desc;
            buffer_desc.element_count = bufferSize;
            buffer_desc.element_size = 1;
            auto dBuffer = Ruzino::cuda::create_cuda_linear_buffer(buffer_desc);

            auto setup_end = std::chrono::high_resolution_clock::now();
            result.setup_time = std::chrono::duration_cast<std::chrono::microseconds>(
                setup_end - start_time);

            // Create borrowed buffers for the implementation
            Ruzino::cuda::CUDALinearBufferDesc vec_existing_desc;
            vec_existing_desc.element_count = n;
            vec_existing_desc.element_size = sizeof(float);
            
            auto d_x_buf = Ruzino::cuda::borrow_cuda_linear_buffer(
                vec_existing_desc, (void*)d_x);
            auto d_b_buf = Ruzino::cuda::borrow_cuda_linear_buffer(
                vec_existing_desc, (void*)d_b);

            // Call BiCGSTAB implementation
            result = performCleanBiCGStabImpl(
                cublasHandle,
                cusparseHandle,
                config,
                n,
                matA_desc,
                dBuffer,
                d_b_buf,
                d_x_buf,
                d_r,
                d_r0,
                d_p_buf,
                d_v,
                d_s,
                d_t,
                vecX,
                vecP,
                vecV,
                vecS,
                vecT);

            // Cleanup
            cusparseDestroyDnVec(vecX);
            cusparseDestroyDnVec(vecB);
            cusparseDestroyDnVec(vecP);
            cusparseDestroyDnVec(vecV);
            cusparseDestroyDnVec(vecS);
            cusparseDestroyDnVec(vecT);
            cusparseDestroySpMat(matA_desc);

            auto end_time = std::chrono::high_resolution_clock::now();
            result.solve_time = std::chrono::duration_cast<std::chrono::microseconds>(
                end_time - setup_end);
        }
        catch (const std::exception& e) {
            result.error_message = std::string("GPU solve failed: ") + e.what();
            result.converged = false;
        }

        return result;
    }

    SolverResult solve(
        const Eigen::SparseMatrix<float>& A,
        const Eigen::VectorXf& b,
        Eigen::VectorXf& x,
        const SolverConfig& config = SolverConfig{}) override
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        SolverResult result;

        try {
            int n = A.rows();
            int nnz = A.nonZeros();

            if (config.verbose) {
                std::cout << "CUDA BiCGSTAB: n=" << n << ", nnz=" << nnz
                          << std::endl;
            }

            // Convert to CSR format
            std::vector<int> csrRowPtr(n + 1, 0);
            std::vector<int> csrColInd(nnz);
            std::vector<float> csrValues(nnz);

            for (int k = 0; k < A.outerSize(); ++k) {
                for (Eigen::SparseMatrix<float>::InnerIterator it(A, k); it; ++it) {
                    csrRowPtr[it.row() + 1]++;
                }
            }
            for (int i = 1; i <= n; ++i) {
                csrRowPtr[i] += csrRowPtr[i - 1];
            }
            std::vector<int> current_pos = csrRowPtr;
            for (int k = 0; k < A.outerSize(); ++k) {
                for (Eigen::SparseMatrix<float>::InnerIterator it(A, k); it; ++it) {
                    int row = it.row();
                    int pos = current_pos[row]++;
                    csrValues[pos] = it.value();
                    csrColInd[pos] = it.col();
                }
            }

            auto setup_end_time = std::chrono::high_resolution_clock::now();
            result.setup_time = std::chrono::duration_cast<std::chrono::microseconds>(
                setup_end_time - start_time);

            // Upload to GPU
            auto d_csrValues = Ruzino::cuda::create_cuda_linear_buffer(csrValues);
            auto d_csrRowPtr = Ruzino::cuda::create_cuda_linear_buffer(csrRowPtr);
            auto d_csrColInd = Ruzino::cuda::create_cuda_linear_buffer(csrColInd);
            auto d_b = Ruzino::cuda::create_cuda_linear_buffer<float>(n);
            auto d_x = Ruzino::cuda::create_cuda_linear_buffer<float>(n);

            d_b->assign_host_vector(std::vector<float>(b.data(), b.data() + b.size()));
            d_x->assign_host_vector(std::vector<float>(x.data(), x.data() + x.size()));

            // Call GPU solve
            result = solveGPU(
                n, nnz,
                reinterpret_cast<const int*>(d_csrRowPtr->get_device_ptr()),
                reinterpret_cast<const int*>(d_csrColInd->get_device_ptr()),
                reinterpret_cast<const float*>(d_csrValues->get_device_ptr()),
                reinterpret_cast<const float*>(d_b->get_device_ptr()),
                reinterpret_cast<float*>(d_x->get_device_ptr()),
                config);

            // Download result
            auto result_vec = d_x->get_host_vector<float>();
            x = Eigen::Map<Eigen::VectorXf>(result_vec.data(), result_vec.size());
        }
        catch (const std::exception& e) {
            result.error_message = e.what();
            result.converged = false;
        }

        return result;
    }

   private:
    SolverResult performCleanBiCGStab(
        const SolverConfig& config,
        int n,
        cusparseSpMatDescr_t matA_desc,
        Ruzino::cuda::CUDALinearBufferHandle dBuffer,
        Ruzino::cuda::CUDALinearBufferHandle d_b,
        Ruzino::cuda::CUDALinearBufferHandle d_x,
        Ruzino::cuda::CUDALinearBufferHandle d_r,
        Ruzino::cuda::CUDALinearBufferHandle d_r0,
        Ruzino::cuda::CUDALinearBufferHandle d_p,
        Ruzino::cuda::CUDALinearBufferHandle d_v,
        Ruzino::cuda::CUDALinearBufferHandle d_s,
        Ruzino::cuda::CUDALinearBufferHandle d_t,
        cusparseDnVecDescr_t vecX_desc,
        cusparseDnVecDescr_t vecP_desc,
        cusparseDnVecDescr_t vecV_desc,
        cusparseDnVecDescr_t vecS_desc,
        cusparseDnVecDescr_t vecT_desc)
    {
        // 委托给静态函数
        return performCleanBiCGStabImpl(
            cublasHandle,
            cusparseHandle,
            config,
            n,
            matA_desc,
            dBuffer,
            d_b,
            d_x,
            d_r,
            d_r0,
            d_p,
            d_v,
            d_s,
            d_t,
            vecX_desc,
            vecP_desc,
            vecV_desc,
            vecS_desc,
            vecT_desc);
    }
};

// Factory registration
std::unique_ptr<LinearSolver> createCudaBiCGStabSolver()
{
    return std::make_unique<CudaBiCGStabSolver>();
}

}  // namespace Solver

RUZINO_NAMESPACE_CLOSE_SCOPE
