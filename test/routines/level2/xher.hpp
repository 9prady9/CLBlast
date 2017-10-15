
// =================================================================================================
// This file is part of the CLBlast project. The project is licensed under Apache Version 2.0. This
// project loosely follows the Google C++ styleguide and uses a tab-size of two spaces and a max-
// width of 100 characters per line.
//
// Author(s):
//   Cedric Nugteren <www.cedricnugteren.nl>
//
// This file implements a class with static methods to describe the Xher routine. Examples of
// such 'descriptions' are how to calculate the size a of buffer or how to run the routine. These
// static methods are used by the correctness tester and the performance tester.
//
// =================================================================================================

#ifndef CLBLAST_TEST_ROUTINES_XHER_H_
#define CLBLAST_TEST_ROUTINES_XHER_H_

#include "test/routines/common.hpp"

namespace clblast {
// =================================================================================================

// See comment at top of file for a description of the class
template <typename T, typename U>
class TestXher {
 public:

  // The BLAS level: 1, 2, or 3
  static size_t BLASLevel() { return 2; }

  // The list of arguments relevant for this routine
  static std::vector<std::string> GetOptions() {
    return {kArgN,
            kArgLayout, kArgTriangle,
            kArgALeadDim, kArgXInc,
            kArgAOffset, kArgXOffset,
            kArgAlpha};
  }
  static std::vector<std::string> BuffersIn() { return {kBufMatA, kBufVecX}; }
  static std::vector<std::string> BuffersOut() { return {kBufMatA}; }

  // Describes how to obtain the sizes of the buffers
  static size_t GetSizeX(const Arguments<U> &args) {
    return args.n * args.x_inc + args.x_offset;
  }
  static size_t GetSizeA(const Arguments<U> &args) {
    return args.n * args.a_ld + args.a_offset;
  }

  // Describes how to set the sizes of all the buffers
  static void SetSizes(Arguments<U> &args) {
    args.a_size = GetSizeA(args);
    args.x_size = GetSizeX(args);
  }

  // Describes what the default values of the leading dimensions of the matrices are
  static size_t DefaultLDA(const Arguments<U> &args) { return args.n; }
  static size_t DefaultLDB(const Arguments<U> &) { return 1; } // N/A for this routine
  static size_t DefaultLDC(const Arguments<U> &) { return 1; } // N/A for this routine

  // Describes which transpose options are relevant for this routine
  using Transposes = std::vector<Transpose>;
  static Transposes GetATransposes(const Transposes &) { return {}; } // N/A for this routine
  static Transposes GetBTransposes(const Transposes &) { return {}; } // N/A for this routine

  // Describes how to prepare the input data
  static void PrepareData(const Arguments<U>&, Queue&, const int, std::vector<T>&,
                          std::vector<T>&, std::vector<T>&, std::vector<T>&, std::vector<T>&,
                          std::vector<T>&, std::vector<T>&) {} // N/A for this routine

  // Describes how to run the CLBlast routine
  static StatusCode RunRoutine(const Arguments<U> &args, Buffers<T> &buffers, Queue &queue) {
    #ifdef OPENCL_API
      auto queue_plain = queue();
      auto event = cl_event{};
      auto status = Her(args.layout, args.triangle,
                        args.n, args.alpha,
                        buffers.x_vec(), args.x_offset, args.x_inc,
                        buffers.a_mat(), args.a_offset, args.a_ld,
                        &queue_plain, &event);
      if (status == StatusCode::kSuccess) { clWaitForEvents(1, &event); clReleaseEvent(event); }
    #elif CUDA_API
      auto status = Her(args.layout, args.triangle,
                        args.n, args.alpha,
                        buffers.x_vec(), args.x_offset, args.x_inc,
                        buffers.a_mat(), args.a_offset, args.a_ld,
                        queue.GetContext()(), queue.GetDevice()());
      cuStreamSynchronize(queue());
    #endif
    return status;
  }

  // Describes how to run the clBLAS routine (for correctness/performance comparison)
  #ifdef CLBLAST_REF_CLBLAS
    static StatusCode RunReference1(const Arguments<U> &args, Buffers<T> &buffers, Queue &queue) {
      auto queue_plain = queue();
      auto event = cl_event{};
      auto status = clblasXher(convertToCLBLAS(args.layout),
                               convertToCLBLAS(args.triangle),
                               args.n, args.alpha,
                               buffers.x_vec, args.x_offset, args.x_inc,
                               buffers.a_mat, args.a_offset, args.a_ld,
                               1, &queue_plain, 0, nullptr, &event);
      clWaitForEvents(1, &event);
      return static_cast<StatusCode>(status);
    }
  #endif

  // Describes how to run the CPU BLAS routine (for correctness/performance comparison)
  #ifdef CLBLAST_REF_CBLAS
    static StatusCode RunReference2(const Arguments<U> &args, BuffersHost<T> &buffers_host, Queue&) {
      cblasXher(convertToCBLAS(args.layout),
                convertToCBLAS(args.triangle),
                args.n, args.alpha,
                buffers_host.x_vec, args.x_offset, args.x_inc,
                buffers_host.a_mat, args.a_offset, args.a_ld);
      return StatusCode::kSuccess;
    }
  #endif

  // Describes how to run the cuBLAS routine (for correctness/performance comparison)
  #ifdef CLBLAST_REF_CUBLAS
    static StatusCode RunReference3(const Arguments<U> &args, BuffersCUDA<T> &buffers, Queue &) {
      auto status = cublasXher(reinterpret_cast<cublasHandle_t>(args.cublas_handle), args.layout,
                               convertToCUBLAS(args.triangle),
                               args.n, args.alpha,
                               buffers.x_vec, args.x_offset, args.x_inc,
                               buffers.a_mat, args.a_offset, args.a_ld);
      if (status == CUBLAS_STATUS_SUCCESS) { return StatusCode::kSuccess; } else { return StatusCode::kUnknownError; }
    }
  #endif

  // Describes how to download the results of the computation (more importantly: which buffer)
  static std::vector<T> DownloadResult(const Arguments<U> &args, Buffers<T> &buffers, Queue &queue) {
    std::vector<T> result(args.a_size, static_cast<T>(0));
    buffers.a_mat.Read(queue, args.a_size, result);
    return result;
  }

  // Describes how to compute the indices of the result buffer
  static size_t ResultID1(const Arguments<U> &args) { return args.n; }
  static size_t ResultID2(const Arguments<U> &args) { return args.n; }
  static size_t GetResultIndex(const Arguments<U> &args, const size_t id1, const size_t id2) {
    return id2*args.a_ld + id1 + args.a_offset;
  }

  // Describes how to compute performance metrics
  static size_t GetFlops(const Arguments<U> &args) {
    return 3 * args.n * args.n;
  }
  static size_t GetBytes(const Arguments<U> &args) {
    return (args.n*args.n + args.n) * sizeof(T);
  }
};

// =================================================================================================
} // namespace clblast

// CLBLAST_TEST_ROUTINES_XHER_H_
#endif
