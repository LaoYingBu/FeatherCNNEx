#if __ARM_NEON
#include <arm_neon.h>
#endif // __ARM_NEON
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include "convdirect.h"

void conv5x5s1_neon(float *input, int inch, int h, int w, int inChannelSize, float *output, int outch, int outh, int outw, int outChannelSize, const float* kernel, const float* bias, unsigned num_threads)
{
    #pragma omp parallel for num_threads(num_threads)
    for (int p=0; p<outch; p++)
    {
        float *out = output + p*outChannelSize;
        const float bias0 = bias ? bias[p] : 0.f;
        fill(out, outChannelSize, bias0);

        for (int q=0; q<inch; q++)
        {
            float* outptr = out;
            float* outptr2 = outptr + outw;

            const float* img0 = input + q*inChannelSize;
            const float* kernel0 = kernel + p*inch*25  + q*25;

            const float* r0 = img0;
            const float* r1 = img0 + w;
            const float* r2 = img0 + w*2;
            const float* r3 = img0 + w*3;
            const float* r4 = img0 + w*4;
            const float* r5 = img0 + w*5;

            const float* k0 = kernel0;
            const float* k1 = kernel0 + 5;
            const float* k2 = kernel0 + 10;
            const float* k3 = kernel0 + 15;
            const float* k4 = kernel0 + 20;

#if __ARM_NEON
            float32x4_t _k0123 = vld1q_f32(kernel0);
            float32x4_t _k4567 = vld1q_f32(kernel0+4);
            float32x4_t _k891011 = vld1q_f32(kernel0+8);
            float32x4_t _k12131415 = vld1q_f32(kernel0+12);
            float32x4_t _k16171819 = vld1q_f32(kernel0+16);
            float32x4_t _k20212223 = vld1q_f32(kernel0+20);
            float32x4_t _k24242424 = vdupq_n_f32(kernel0[24]);
#endif // __ARM_NEON

            int i = 0;

            for (; i+1 < outh; i+=2)
            {

#if __ARM_NEON
                int nn = outw >> 2;
                int remain = outw - (nn << 2);
#else
                int remain = outw;
#endif // __ARM_NEON

#if __ARM_NEON
#if __aarch64__
                if (nn > 0)
                {
                    asm volatile(
                        // v11 = rx1 / rx3
                        // v12 = rx2
                        // v13 v14 = intermediate sum register

                        "prfm       pldl1keep, [%1, #128]          \n"
                        "ld1        {v7.4s}, [%1]                  \n"// v7 = out

                        "0:                                        \n"

                        "prfm       pldl1keep, [%2, #128]          \n"
                        "ld1        {v8.4s}, [%2]                  \n"// v8 = out2

                        // r1
                        "prfm       pldl1keep, [%4, #256]          \n"
                        "ld1        {v9.4s, v10.4s}, [%4]          \n"// v9 v10 = r10 r14
                        "add        %4, %4, #16                    \n"

                        "ext        v11.16b, v9.16b, v10.16b, #4   \n" //r11
                        "fmul       v13.4s, v9.4s, %19.s[1]        \n"
                        "fmla       v8.4s,  v9.4s, %18.s[0]        \n"

                        "ext        v12.16b, v9.16b, v10.16b, #8   \n" //r12
                        "fmla       v7.4s,  v11.4s, %19.s[2]       \n"
                        "fmul       v14.4s, v11.4s, %18.s[1]       \n"

                        "ext        v11.16b, v9.16b, v10.16b, #12  \n" //r13
                        "fmla       v13.4s, v12.4s, %19.s[3]       \n"
                        "fmla       v8.4s,  v12.4s, %18.s[2]       \n"

                        "fmla       v7.4s,  v11.4s, %20.s[0]       \n"
                        "fmla       v14.4s, v11.4s, %18.s[3]       \n"

                        "prfm       pldl1keep, [%5, #256]          \n"

                        "fmla       v13.4s, v10.4s, %20.s[1]       \n"
                        "fmla       v8.4s,  v10.4s, %19.s[0]       \n"

                        // r2
                        "ld1        {v9.4s, v10.4s}, [%5]          \n"// v9 v10 = r20 r24
                        "add        %5, %5, #16                    \n"

                        "ext        v11.16b, v9.16b, v10.16b, #4   \n" //r21
                        "fmla       v7.4s,  v9.4s, %20.s[2]        \n"
                        "fmla       v14.4s, v9.4s, %19.s[1]        \n"

                        "ext        v12.16b, v9.16b, v10.16b, #8   \n" //r22
                        "fmla       v13.4s, v11.4s, %20.s[3]       \n"
                        "fmla       v8.4s,  v11.4s, %19.s[2]       \n"

                        "ext        v11.16b, v9.16b, v10.16b, #12  \n" //r23
                        "fmla       v7.4s,  v12.4s, %21.s[0]       \n"
                        "fmla       v14.4s, v12.4s, %19.s[3]       \n"

                        "fmla       v13.4s, v11.4s, %21.s[1]       \n"
                        "fmla       v8.4s,  v11.4s, %20.s[0]       \n"

                        "prfm       pldl1keep, [%6, #256]          \n"

                        "fmla       v7.4s,  v10.4s, %21.s[2]       \n"
                        "fmla       v14.4s, v10.4s, %20.s[1]       \n"

                        // r3
                        "ld1        {v9.4s, v10.4s}, [%6]          \n"// v9 v10 = r30 r34
                        "add        %6, %6, #16                    \n"

                        "ext        v11.16b, v9.16b, v10.16b, #4   \n" //r31
                        "fmla       v13.4s, v9.4s, %21.s[3]        \n"
                        "fmla       v8.4s,  v9.4s, %20.s[2]        \n"

                        "ext        v12.16b, v9.16b, v10.16b, #8   \n" //r32
                        "fmla       v7.4s,  v11.4s, %22.s[0]       \n"
                        "fmla       v14.4s, v11.4s, %20.s[3]       \n"

                        "ext        v11.16b, v9.16b, v10.16b, #12  \n" //r33
                        "fmla       v13.4s, v12.4s, %22.s[1]       \n"
                        "fmla       v8.4s,  v12.4s, %21.s[0]       \n"

                        "fmla       v7.4s,  v11.4s, %22.s[2]       \n"
                        "fmla       v14.4s, v11.4s, %21.s[1]       \n"

                        "prfm       pldl1keep, [%7, #256]          \n"

                        "fmla       v13.4s, v10.4s, %22.s[3]       \n"
                        "fmla       v8.4s,  v10.4s, %21.s[2]       \n"

                        // r4
                        "ld1        {v9.4s, v10.4s}, [%7]          \n"// v9 v10 = r40 r44
                        "add        %7, %7, #16                    \n"

                        "ext        v11.16b, v9.16b, v10.16b, #4   \n" //r41
                        "fmla       v7.4s,  v9.4s, %23.s[0]        \n"
                        "fmla       v14.4s, v9.4s, %21.s[3]        \n"

                        "ext        v12.16b, v9.16b, v10.16b, #8   \n" //r41
                        "fmla       v13.4s, v11.4s, %23.s[1]       \n"
                        "fmla       v8.4s,  v11.4s, %22.s[0]       \n"

                        "ext        v11.16b, v9.16b, v10.16b, #12  \n" //r41
                        "fmla       v7.4s,  v12.4s, %23.s[2]       \n"
                        "fmla       v14.4s, v12.4s, %22.s[1]       \n"

                        "fmla       v13.4s, v11.4s, %23.s[3]       \n"
                        "fmla       v8.4s,  v11.4s, %22.s[2]       \n"

                        "prfm       pldl1keep, [%3, #256]          \n"

                        "fmla       v7.4s,  v10.4s, %24.s[0]       \n"
                        "fmla       v14.4s, v10.4s, %22.s[3]       \n"

                        // r0 and r5
                        "ld1        {v9.4s, v10.4s}, [%3]          \n"// v9 v10 = r00 r04
                        "add        %3, %3, #16                    \n"

                        "ext        v11.16b, v9.16b, v10.16b, #4   \n" //r01
                        "fmla       v13.4s, v11.4s, %18.s[1]       \n"

                        "ext        v12.16b, v9.16b, v10.16b, #8   \n" //r02
                        "fmla       v7.4s, v12.4s, %18.s[2]        \n"

                        "ext        v11.16b, v9.16b, v10.16b, #12  \n" //r03

                        "prfm       pldl1keep, [%8, #256]          \n"

                        "fmla       v13.4s, v11.4s, %18.s[3]       \n"

                        // r5
                        "ld1        {v11.4s, v12.4s}, [%8]         \n"// v11 v12 = r50 r54
                        "add        %8, %8, #16                    \n"

                        "fmla       v8.4s,  v11.4s, %23.s[0]       \n"
                        "fmla       v14.4s, v12.4s, %24.s[0]       \n"

                        "fmla       v7.4s,  v9.4s,  %18.s[0]       \n"
                        "fmla       v13.4s, v10.4s, %19.s[0]       \n"

                        "ext        v9.16b,  v11.16b, v12.16b, #4  \n" //r51
                        "ext        v10.16b, v11.16b, v12.16b, #8  \n" //r52

                        "fmla       v14.4s, v9.4s, %23.s[1]        \n"

                        "ext        v9.16b, v11.16b, v12.16b, #12  \n" //r53
                        "fmla       v8.4s, v10.4s, %23.s[2]        \n"

                        "fmla       v14.4s, v9.4s, %23.s[3]        \n"

                        "fadd       v7.4s, v7.4s, v13.4s           \n"

                        "st1        {v7.4s}, [%1], #16             \n"

                        "fadd       v8.4s, v8.4s, v14.4s           \n"

                        "prfm       pldl1keep, [%1, #128]          \n"
                        "ld1        {v7.4s}, [%1]                  \n"// v7 = out
                        "st1        {v8.4s}, [%2], #16             \n"

                        "subs       %w0, %w0, #1                   \n"
                        "bne        0b                             \n"
                        : "=r"(nn),         // %0
                        "=r"(outptr),     // %1
                        "=r"(outptr2),    // %2
                        "=r"(r0),         // %3
                        "=r"(r1),         // %4
                        "=r"(r2),         // %5
                        "=r"(r3),         // %6
                        "=r"(r4),         // %7
                        "=r"(r5)          // %8
                        : "0"(nn),
                        "1"(outptr),
                        "2"(outptr2),
                        "3"(r0),
                        "4"(r1),
                        "5"(r2),
                        "6"(r3),
                        "7"(r4),
                        "8"(r5),
                        "w"(_k0123),      // %18
                        "w"(_k4567),      // %19
                        "w"(_k891011),    // %20
                        "w"(_k12131415),  // %21
                        "w"(_k16171819),  // %22
                        "w"(_k20212223),  // %23
                        "w"(_k24242424)   // %24
                        : "cc", "memory", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15"
                    );
                }
#else
                if (nn > 0)
                {
                    asm volatile(
//                     "veor       q13, q13            \n"
//                     "veor       q14, q14            \n"

                        "pld        [%1, #128]          \n"

                        "vld1.f32   {d14-d15}, [%1]     \n"// q7 = out

                        "0:                             \n"

                        // q11 = rx1 / rx3
                        // q12 = rx2

                        // q13 q14 = intermediate sum register

                        "pld        [%2, #128]          \n"

                        "vld1.f32   {d16-d17}, [%2]     \n"// q8 = out2


                        "pld        [%4, #256]          \n"

                        // r1
                        "vld1.f32   {d18-d21}, [%4]     \n"// q9 q10 = r10 r14
                        "add        %4, #16             \n"

                        "vext.32    q11, q9, q10, #1    \n"// r11
                        "vmul.f32   q13, q9, %e19[1]    \n"
                        "vmla.f32   q8, q9, %e18[0]     \n"

                        "vext.32    q12, q9, q10, #2    \n"// r12
                        "vmla.f32   q7, q11, %f19[0]    \n"
                        "vmul.f32   q14, q11, %e18[1]   \n"

                        "vext.32    q11, q9, q10, #3    \n"// r13
                        "vmla.f32   q13, q12, %f19[1]   \n"
                        "vmla.f32   q8, q12, %f18[0]    \n"

                        "vmla.f32   q7, q11, %e20[0]    \n"
                        "vmla.f32   q14, q11, %f18[1]   \n"

                        "pld        [%5, #256]          \n"

                        "vmla.f32   q13, q10, %e20[1]   \n"
                        "vmla.f32   q8, q10, %e19[0]    \n"

                        // r2
                        "vld1.f32   {d18-d21}, [%5]     \n"// q9 q10 = r20 r24
                        "add        %5, #16             \n"

                        "vext.32    q11, q9, q10, #1    \n"// r21
                        "vmla.f32   q7, q9, %f20[0]     \n"
                        "vmla.f32   q14, q9, %e19[1]    \n"

                        "vext.32    q12, q9, q10, #2    \n"// r22
                        "vmla.f32   q13, q11, %f20[1]   \n"
                        "vmla.f32   q8, q11, %f19[0]    \n"

                        "vext.32    q11, q9, q10, #3    \n"// r23
                        "vmla.f32   q7, q12, %e21[0]    \n"
                        "vmla.f32   q14, q12, %f19[1]   \n"

                        "vmla.f32   q13, q11, %e21[1]   \n"
                        "vmla.f32   q8, q11, %e20[0]    \n"

                        "pld        [%6, #256]          \n"

                        "vmla.f32   q7, q10, %f21[0]    \n"
                        "vmla.f32   q14, q10, %e20[1]   \n"

                        // r3
                        "vld1.f32   {d18-d21}, [%6]     \n"// q9 q10 = r30 r34
                        "add        %6, #16             \n"

                        "vext.32    q11, q9, q10, #1    \n"// r31
                        "vmla.f32   q13, q9, %f21[1]    \n"
                        "vmla.f32   q8, q9, %f20[0]     \n"

                        "vext.32    q12, q9, q10, #2    \n"// r32
                        "vmla.f32   q7, q11, %e22[0]    \n"
                        "vmla.f32   q14, q11, %f20[1]   \n"

                        "vext.32    q11, q9, q10, #3    \n"// r33
                        "vmla.f32   q13, q12, %e22[1]   \n"
                        "vmla.f32   q8, q12, %e21[0]    \n"

                        "vmla.f32   q7, q11, %f22[0]    \n"
                        "vmla.f32   q14, q11, %e21[1]   \n"

                        "pld        [%7, #256]          \n"

                        "vmla.f32   q13, q10, %f22[1]   \n"
                        "vmla.f32   q8, q10, %f21[0]    \n"

                        // r4
                        "vld1.f32   {d18-d21}, [%7]     \n"// q9 q10 = r40 r44
                        "add        %7, #16             \n"

                        "vext.32    q11, q9, q10, #1    \n"// r41
                        "vmla.f32   q7, q9, %e23[0]     \n"
                        "vmla.f32   q14, q9, %f21[1]    \n"

                        "vext.32    q12, q9, q10, #2    \n"// r42
                        "vmla.f32   q13, q11, %e23[1]   \n"
                        "vmla.f32   q8, q11, %e22[0]    \n"

                        "vext.32    q11, q9, q10, #3    \n"// r43
                        "vmla.f32   q7, q12, %f23[0]    \n"
                        "vmla.f32   q14, q12, %e22[1]   \n"

                        "vmla.f32   q13, q11, %f23[1]   \n"
                        "vmla.f32   q8, q11, %f22[0]    \n"

                        "pld        [%3, #256]          \n"

                        "vmla.f32   q7, q10, %e24[0]    \n"
                        "vmla.f32   q14, q10, %f22[1]   \n"

                        // r0 and r5
                        "vld1.f32   {d18-d21}, [%3]     \n"// q9 q10 = r00 r04
                        "add        %3, #16             \n"

                        "vext.32    q11, q9, q10, #1    \n"// r01
                        "vmla.f32   q13, q11, %e18[1]   \n"

                        "vext.32    q12, q9, q10, #2    \n"// r02
                        "vmla.f32   q7, q12, %f18[0]    \n"

                        "vext.32    q11, q9, q10, #3    \n"// r03

                        "pld        [%8, #256]          \n"

                        "vmla.f32   q13, q11, %f18[1]   \n"

                        // r5
                        "vld1.f32   {d22-d25}, [%8]     \n"// q11 q12 = r50 r54
                        "add        %8, #16             \n"

                        "vmla.f32   q8, q11, %e23[0]    \n"
                        "vmla.f32   q14, q12, %e24[0]   \n"

                        "vmla.f32   q7, q9, %e18[0]     \n"
                        "vmla.f32   q13, q10, %e19[0]   \n"

                        "vext.32    q9, q11, q12, #1    \n"// r51
                        "vext.32    q10, q11, q12, #2   \n"// r52

                        "vmla.f32   q14, q9, %e23[1]    \n"

                        "vext.32    q9, q11, q12, #3    \n"// r53
                        "vmla.f32   q8, q10, %f23[0]    \n"

                        "vmla.f32   q14, q9, %f23[1]    \n"

                        "vadd.f32   q7, q7, q13         \n"

//                     "veor       q13, q13            \n"

                        "vst1.f32   {d14-d15}, [%1]!    \n"

                        "vadd.f32   q8, q8, q14         \n"

                        "pld        [%1, #128]          \n"

                        "vld1.f32   {d14-d15}, [%1]     \n"// q7 = out

//                     "veor       q14, q14            \n"

                        "vst1.f32   {d16-d17}, [%2]!    \n"

                        "subs       %0, #1              \n"
                        "bne        0b                  \n"
                        : "=r"(nn),         // %0
                        "=r"(outptr),     // %1
                        "=r"(outptr2),    // %2
                        "=r"(r0),         // %3
                        "=r"(r1),         // %4
                        "=r"(r2),         // %5
                        "=r"(r3),         // %6
                        "=r"(r4),         // %7
                        "=r"(r5)          // %8
                        : "0"(nn),
                        "1"(outptr),
                        "2"(outptr2),
                        "3"(r0),
                        "4"(r1),
                        "5"(r2),
                        "6"(r3),
                        "7"(r4),
                        "8"(r5),
                        "w"(_k0123),      // %18
                        "w"(_k4567),      // %19
                        "w"(_k891011),    // %20
                        "w"(_k12131415),  // %21
                        "w"(_k16171819),  // %22
                        "w"(_k20212223),  // %23
                        "w"(_k24242424)   // %24
                        : "cc", "memory", "q7", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15"
                    );
                }
#endif // __aarch64__
#endif // __ARM_NEON
                for (; remain>0; remain--)
                {
                    float sum = 0;
                    float sum2 = 0;
#if __ARM_NEON
                    float32x4_t _r1 = vld1q_f32(r1);
                    float32x4_t _k1 = vld1q_f32(k1);
                    float32x4_t _sum = vmulq_f32(_r1, _k1);
                    float32x4_t _sum2 = vmulq_f32(_r1, _k0123);

                    float32x4_t _r2 = vld1q_f32(r2);
                    float32x4_t _k2 = vld1q_f32(k2);
                    _sum = vmlaq_f32(_sum, _r2, _k2);
                    _sum2 = vmlaq_f32(_sum2, _r2, _k1);

                    float32x4_t _r3 = vld1q_f32(r3);
                    float32x4_t _k3 = vld1q_f32(k3);
                    _sum = vmlaq_f32(_sum, _r3, _k3);
                    _sum2 = vmlaq_f32(_sum2, _r3, _k2);

                    float32x4_t _r4 = vld1q_f32(r4);
                    _sum = vmlaq_f32(_sum, _r4, _k20212223);
                    _sum2 = vmlaq_f32(_sum2, _r4, _k3);

                    float32x4_t _r0 = vld1q_f32(r0);
                    _sum = vmlaq_f32(_sum, _r0, _k0123);
                    float32x4_t _r5 = vld1q_f32(r5);
                    _sum2 = vmlaq_f32(_sum2, _r5, _k20212223);

                    float32x4_t _k_t4;
                    _k_t4 = vsetq_lane_f32(k0[4], _k_t4, 0);
                    _k_t4 = vsetq_lane_f32(k1[4], _k_t4, 1);
                    _k_t4 = vsetq_lane_f32(k2[4], _k_t4, 2);
                    _k_t4 = vsetq_lane_f32(k3[4], _k_t4, 3);

                    float32x4_t _r_t4;

                    _r_t4 = vsetq_lane_f32(r0[4], _r_t4, 0);
                    _r_t4 = vsetq_lane_f32(r1[4], _r_t4, 1);
                    _r_t4 = vsetq_lane_f32(r2[4], _r_t4, 2);
                    _r_t4 = vsetq_lane_f32(r3[4], _r_t4, 3);
                    _sum = vmlaq_f32(_sum, _r_t4, _k_t4);

                    sum = r4[4] * k4[4];

                    _r_t4 = vextq_f32(_r_t4, _r_t4, 1);
                    _r_t4 = vsetq_lane_f32(r4[4], _r_t4, 3);
                    _sum2 = vmlaq_f32(_sum2, _r_t4, _k_t4);

                    sum2 = r5[4] * k4[4];

                    float32x2_t _ss = vadd_f32(vget_low_f32(_sum), vget_high_f32(_sum));
                    float32x2_t _ss2 = vadd_f32(vget_low_f32(_sum2), vget_high_f32(_sum2));
                    float32x2_t _ss_ss2 = vpadd_f32(_ss, _ss2);

                    sum += vget_lane_f32(_ss_ss2, 0);
                    sum2 += vget_lane_f32(_ss_ss2, 1);
#else
                    sum += r0[0] * k0[0];
                    sum += r0[1] * k0[1];
                    sum += r0[2] * k0[2];
                    sum += r0[3] * k0[3];
                    sum += r0[4] * k0[4];

                    sum += r1[0] * k1[0];
                    sum += r1[1] * k1[1];
                    sum += r1[2] * k1[2];
                    sum += r1[3] * k1[3];
                    sum += r1[4] * k1[4];

                    sum += r2[0] * k2[0];
                    sum += r2[1] * k2[1];
                    sum += r2[2] * k2[2];
                    sum += r2[3] * k2[3];
                    sum += r2[4] * k2[4];

                    sum += r3[0] * k3[0];
                    sum += r3[1] * k3[1];
                    sum += r3[2] * k3[2];
                    sum += r3[3] * k3[3];
                    sum += r3[4] * k3[4];

                    sum += r4[0] * k4[0];
                    sum += r4[1] * k4[1];
                    sum += r4[2] * k4[2];
                    sum += r4[3] * k4[3];
                    sum += r4[4] * k4[4];

                    sum2 += r1[0] * k0[0];
                    sum2 += r1[1] * k0[1];
                    sum2 += r1[2] * k0[2];
                    sum2 += r1[3] * k0[3];
                    sum2 += r1[4] * k0[4];

                    sum2 += r2[0] * k1[0];
                    sum2 += r2[1] * k1[1];
                    sum2 += r2[2] * k1[2];
                    sum2 += r2[3] * k1[3];
                    sum2 += r2[4] * k1[4];

                    sum2 += r3[0] * k2[0];
                    sum2 += r3[1] * k2[1];
                    sum2 += r3[2] * k2[2];
                    sum2 += r3[3] * k2[3];
                    sum2 += r3[4] * k2[4];

                    sum2 += r4[0] * k3[0];
                    sum2 += r4[1] * k3[1];
                    sum2 += r4[2] * k3[2];
                    sum2 += r4[3] * k3[3];
                    sum2 += r4[4] * k3[4];

                    sum2 += r5[0] * k4[0];
                    sum2 += r5[1] * k4[1];
                    sum2 += r5[2] * k4[2];
                    sum2 += r5[3] * k4[3];
                    sum2 += r5[4] * k4[4];
#endif // __ARM_NEON
                    *outptr += sum;
                    *outptr2 += sum2;

                    r0++;
                    r1++;
                    r2++;
                    r3++;
                    r4++;
                    r5++;
                    outptr++;
                    outptr2++;
                }

                r0 += 4 + w;
                r1 += 4 + w;
                r2 += 4 + w;
                r3 += 4 + w;
                r4 += 4 + w;
                r5 += 4 + w;

                outptr += outw;
                outptr2 += outw;
            }

            for (; i < outh; i++)
            {

#if __ARM_NEON
                int nn = outw >> 2;
                int remain = outw - (nn << 2);
#else
                int remain = outw;
#endif // __ARM_NEON

#if __ARM_NEON
#if __aarch64__
                if (nn > 0)
                {
                    asm volatile(
                        "prfm       pldl1keep, [%1, #128]          \n"
                        "prfm       pldl1keep, [%2, #256]          \n"

                        "ld1        {v8.4s, v9.4s}, [%2]           \n"// _r00 = vld1q_f32(r0+j);
                        "add        %2, %2, #16                    \n"

                        "0:                                        \n"

                        "ld1        {v7.4s}, [%1]                  \n"// _sum = vld1q_f32(outptr+j);

                        "ext        v10.16b, v8.16b, v9.16b, #4    \n" //_r01
                        "ext        v11.16b, v8.16b, v9.16b, #8    \n" //_r02
                        "ext        v12.16b, v8.16b, v9.16b, #12   \n" //_r03

                        "fmla       v7.4s,   v8.4s, %14.s[0]       \n"
                        "fmul       v13.4s, v10.4s, %14.s[1]       \n"

                        "prfm       pldl1keep, [%3, #256]          \n"

                        "fmul       v14.4s, v11.4s, %14.s[2]       \n"
                        "fmul       v15.4s, v12.4s, %14.s[3]       \n"
                        "fmla       v7.4s,   v9.4s, %15.s[0]       \n"

                        "ld1        {v8.4s, v9.4s}, [%3]           \n"
                        "add        %3, %3, #16                    \n"
                        "ext        v10.16b, v8.16b, v9.16b, #4    \n" //_r11
                        "ext        v11.16b, v8.16b, v9.16b, #8    \n" //_r12
                        "ext        v12.16b, v8.16b, v9.16b, #12   \n" //_r13

                        "fmla       v7.4s,   v8.4s, %15.s[1]       \n"
                        "fmla       v13.4s, v10.4s, %15.s[2]       \n"

                        "prfm       pldl1keep, [%4, #256]          \n"

                        "fmla       v14.4s, v11.4s, %15.s[3]       \n"
                        "fmla       v15.4s, v12.4s, %16.s[0]       \n"
                        "fmla       v7.4s,   v9.4s, %16.s[1]       \n"

                        "ld1        {v8.4s, v9.4s}, [%4]           \n"
                        "add        %4, %4, #16                    \n"
                        "ext        v10.16b, v8.16b, v9.16b, #4    \n" //_r21
                        "ext        v11.16b, v8.16b, v9.16b, #8    \n" //_r22
                        "ext        v12.16b, v8.16b, v9.16b, #12   \n" //_r23

                        "fmla       v7.4s,   v8.4s, %16.s[2]       \n"
                        "fmla       v13.4s, v10.4s, %16.s[3]       \n"

                        "prfm       pldl1keep, [%5, #256]          \n"

                        "fmla       v14.4s, v11.4s, %17.s[0]       \n"
                        "fmla       v15.4s, v12.4s, %17.s[1]       \n"
                        "fmla       v7.4s,   v9.4s, %17.s[2]       \n"

                        "ld1        {v8.4s, v9.4s}, [%5]           \n"
                        "add        %5, %5, #16                    \n"
                        "ext        v10.16b, v8.16b, v9.16b, #4    \n" //_r31
                        "ext        v11.16b, v8.16b, v9.16b, #8    \n" //_r32
                        "ext        v12.16b, v8.16b, v9.16b, #12   \n" //_r33

                        "fmla       v7.4s,   v8.4s, %17.s[3]       \n"
                        "fmla       v13.4s, v10.4s, %18.s[0]       \n"

                        "prfm       pldl1keep, [%6, #256]          \n"

                        "fmla       v14.4s, v11.4s, %18.s[1]       \n"
                        "fmla       v15.4s, v12.4s, %18.s[2]       \n"
                        "fmla       v7.4s,   v9.4s, %18.s[3]       \n"

                        "ld1        {v8.4s, v9.4s}, [%6]           \n"
                        "add        %6, %6, #16                    \n"
                        "ext        v10.16b, v8.16b, v9.16b, #4    \n" //_r41
                        "ext        v11.16b, v8.16b, v9.16b, #8    \n" //_r42
                        "ext        v12.16b, v8.16b, v9.16b, #12   \n" //_r43

                        "fmla       v7.4s,   v8.4s, %19.s[0]       \n"
                        "fmla       v13.4s, v10.4s, %19.s[1]       \n"
                        "fmla       v14.4s, v11.4s, %19.s[2]       \n"
                        "fmla       v15.4s, v12.4s, %19.s[3]       \n"
                        "fmla       v7.4s,   v9.4s, %20.s[0]       \n"

                        "fadd       v14.4s, v14.4s, v15.4s         \n"
                        "fadd       v7.4s,   v7.4s, v13.4s         \n"

                        "prfm       pldl1keep, [%2, #256]          \n"

                        "fadd       v7.4s,   v7.4s, v14.4s         \n"

                        "ld1        {v8.4s, v9.4s}, [%2]           \n"
                        "add        %2, %2, #16                    \n"

                        "st1        {v7.4s}, [%1], #16             \n"

                        "prfm       pldl1keep, [%1, #128]          \n"

                        "subs       %w0, %w0, #1                   \n"
                        "bne        0b                             \n"

                        "sub        %2, %2, #16                    \n"
                        : "=r"(nn),         // %0
                        "=r"(outptr),     // %1
                        "=r"(r0),         // %2
                        "=r"(r1),         // %3
                        "=r"(r2),         // %4
                        "=r"(r3),         // %5
                        "=r"(r4)          // %6
                        : "0"(nn),
                        "1"(outptr),
                        "2"(r0),
                        "3"(r1),
                        "4"(r2),
                        "5"(r3),
                        "6"(r4),
                        "w"(_k0123),      // %14
                        "w"(_k4567),      // %15
                        "w"(_k891011),    // %16
                        "w"(_k12131415),  // %17
                        "w"(_k16171819),  // %18
                        "w"(_k20212223),  // %19
                        "w"(_k24242424)   // %20
                        : "cc", "memory", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15"
                    );
                }
#else
                if (nn > 0)
                {
                    asm volatile(
//                     "veor       q15, q15            \n"// _sum3 = 0;

                        "pld        [%1, #128]          \n"

                        "pld        [%2, #256]          \n"

                        "vld1.f32   {d16-d19}, [%2]     \n"// _r00 = vld1q_f32(r0+j);
                        "add        %2, #16             \n"

                        "0:                             \n"

                        "vld1.f32   {d14-d15}, [%1]     \n"// _sum = vld1q_f32(outptr+j);
//                     "veor       q13, q13            \n"// _sum2 = 0;
//                     "veor       q14, q14            \n"// _sum3 = 0;

                        "vext.32    q10, q8, q9, #1     \n"// _r01
                        "vext.32    q11, q8, q9, #2     \n"// _r02
                        "vext.32    q12, q8, q9, #3     \n"// _r03

                        "vmla.f32   q7, q8, %e14[0]     \n"
                        "vmul.f32   q13, q10, %e14[1]   \n"

                        "pld        [%3, #256]          \n"

                        "vmul.f32   q14, q11, %f14[0]   \n"
                        "vmul.f32   q15, q12, %f14[1]   \n"
                        "vmla.f32   q7, q9, %e15[0]     \n"

                        "vld1.f32   {d16-d19}, [%3]     \n"
                        "add        %3, #16             \n"
                        "vext.32    q10, q8, q9, #1     \n"
                        "vext.32    q11, q8, q9, #2     \n"
                        "vext.32    q12, q8, q9, #3     \n"

                        "vmla.f32   q7, q8, %e15[1]     \n"
                        "vmla.f32   q13, q10, %f15[0]   \n"

                        "pld        [%4, #256]          \n"

                        "vmla.f32   q14, q11, %f15[1]   \n"
                        "vmla.f32   q15, q12, %e16[0]   \n"
                        "vmla.f32   q7, q9, %e16[1]     \n"

                        "vld1.f32   {d16-d19}, [%4]     \n"
                        "add        %4, #16             \n"
                        "vext.32    q10, q8, q9, #1     \n"
                        "vext.32    q11, q8, q9, #2     \n"
                        "vext.32    q12, q8, q9, #3     \n"

                        "vmla.f32   q7, q8, %f16[0]     \n"
                        "vmla.f32   q13, q10, %f16[1]   \n"

                        "pld        [%5, #256]          \n"

                        "vmla.f32   q14, q11, %e17[0]   \n"
                        "vmla.f32   q15, q12, %e17[1]   \n"
                        "vmla.f32   q7, q9, %f17[0]     \n"

                        "vld1.f32   {d16-d19}, [%5]     \n"
                        "add        %5, #16             \n"
                        "vext.32    q10, q8, q9, #1     \n"
                        "vext.32    q11, q8, q9, #2     \n"
                        "vext.32    q12, q8, q9, #3     \n"

                        "vmla.f32   q7, q8, %f17[1]     \n"
                        "vmla.f32   q13, q10, %e18[0]   \n"

                        "pld        [%6, #256]          \n"

                        "vmla.f32   q14, q11, %e18[1]   \n"
                        "vmla.f32   q15, q12, %f18[0]   \n"
                        "vmla.f32   q7, q9, %f18[1]     \n"

                        "vld1.f32   {d16-d19}, [%6]     \n"
                        "add        %6, #16             \n"
                        "vext.32    q10, q8, q9, #1     \n"
                        "vext.32    q11, q8, q9, #2     \n"
                        "vext.32    q12, q8, q9, #3     \n"

                        "vmla.f32   q7, q8, %e19[0]     \n"
                        "vmla.f32   q13, q10, %e19[1]   \n"
                        "vmla.f32   q14, q11, %f19[0]   \n"
                        "vmla.f32   q15, q12, %f19[1]   \n"
                        "vmla.f32   q7, q9, %e20[0]     \n"

                        "vadd.f32   q14, q14, q15       \n"
                        "vadd.f32   q7, q7, q13         \n"
//                     "veor       q15, q15            \n"// _sum3 = 0;

                        "pld        [%2, #256]          \n"

                        "vadd.f32   q7, q7, q14         \n"

                        "vld1.f32   {d16-d19}, [%2]     \n"// _r00 = vld1q_f32(r0+j);
                        "add        %2, #16             \n"

                        "vst1.f32   {d14-d15}, [%1]!    \n"

                        "pld        [%1, #128]          \n"

                        "subs       %0, #1              \n"
                        "bne        0b                  \n"

                        "sub        %2, #16             \n"
                        : "=r"(nn),         // %0
                        "=r"(outptr),     // %1
                        "=r"(r0),         // %2
                        "=r"(r1),         // %3
                        "=r"(r2),         // %4
                        "=r"(r3),         // %5
                        "=r"(r4)          // %6
                        : "0"(nn),
                        "1"(outptr),
                        "2"(r0),
                        "3"(r1),
                        "4"(r2),
                        "5"(r3),
                        "6"(r4),
                        "w"(_k0123),      // %14
                        "w"(_k4567),      // %15
                        "w"(_k891011),    // %16
                        "w"(_k12131415),  // %17
                        "w"(_k16171819),  // %18
                        "w"(_k20212223),  // %19
                        "w"(_k24242424)   // %20
                        : "cc", "memory", "q7", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15"
                    );
                }
#endif // __aarch64__
#endif // __ARM_NEON
                for (; remain>0; remain--)
                {
                    float sum = 0;
#if __ARM_NEON
                    float32x4_t _r0 = vld1q_f32(r0);
                    float32x4_t _sum = vmulq_f32(_r0, _k0123);

                    float32x4_t _r1 = vld1q_f32(r1);
                    _sum = vmlaq_f32(_sum, _r1, vld1q_f32(k1));

                    float32x4_t _r2 = vld1q_f32(r2);
                    _sum = vmlaq_f32(_sum, _r2, vld1q_f32(k2));

                    float32x4_t _r3 = vld1q_f32(r3);
                    _sum = vmlaq_f32(_sum, _r3, vld1q_f32(k3));

                    float32x4_t _r4 = vld1q_f32(r4);
                    _sum = vmlaq_f32(_sum, _r4, _k20212223);

                    float32x4_t _k_t4;
                    _k_t4 = vsetq_lane_f32(k0[4], _k_t4, 0);
                    _k_t4 = vsetq_lane_f32(k1[4], _k_t4, 1);
                    _k_t4 = vsetq_lane_f32(k2[4], _k_t4, 2);
                    _k_t4 = vsetq_lane_f32(k3[4], _k_t4, 3);

                    float32x4_t _r_t4;

                    _r_t4 = vsetq_lane_f32(r0[4], _r_t4, 0);
                    _r_t4 = vsetq_lane_f32(r1[4], _r_t4, 1);
                    _r_t4 = vsetq_lane_f32(r2[4], _r_t4, 2);
                    _r_t4 = vsetq_lane_f32(r3[4], _r_t4, 3);
                    _sum = vmlaq_f32(_sum, _r_t4, _k_t4);

                    sum = r4[4] * k4[4];

                    float32x2_t _ss = vadd_f32(vget_low_f32(_sum), vget_high_f32(_sum));
                    _ss = vpadd_f32(_ss, _ss);

                    sum += vget_lane_f32(_ss, 0);
#else
                    sum += r0[0] * k0[0];
                    sum += r0[1] * k0[1];
                    sum += r0[2] * k0[2];
                    sum += r0[3] * k0[3];
                    sum += r0[4] * k0[4];

                    sum += r1[0] * k1[0];
                    sum += r1[1] * k1[1];
                    sum += r1[2] * k1[2];
                    sum += r1[3] * k1[3];
                    sum += r1[4] * k1[4];

                    sum += r2[0] * k2[0];
                    sum += r2[1] * k2[1];
                    sum += r2[2] * k2[2];
                    sum += r2[3] * k2[3];
                    sum += r2[4] * k2[4];

                    sum += r3[0] * k3[0];
                    sum += r3[1] * k3[1];
                    sum += r3[2] * k3[2];
                    sum += r3[3] * k3[3];
                    sum += r3[4] * k3[4];

                    sum += r4[0] * k4[0];
                    sum += r4[1] * k4[1];
                    sum += r4[2] * k4[2];
                    sum += r4[3] * k4[3];
                    sum += r4[4] * k4[4];
#endif
                    *outptr += sum;

                    r0++;
                    r1++;
                    r2++;
                    r3++;
                    r4++;
                    outptr++;
                }

                r0 += 4;
                r1 += 4;
                r2 += 4;
                r3 += 4;
                r4 += 4;

            }
        }
    }
}
