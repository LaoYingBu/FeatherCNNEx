#pragma once

void conv3x3s1_neon(float *input, int inch, int h, int w, int inChannelSize, float *output, int outch, int outh, int outw, int outChannelSize, const float* kernel, const float* bias, unsigned num_threads);
void conv3x3s2_neon(float *input, int inch, int h, int w, int inChannelSize, float *output, int outch, int outh, int outw, int outChannelSize, const float* kernel, const float* bias, unsigned num_threads);
void conv1x1s1_neon(float *input, int inch, int h, int w, int inChannelSize, float *output, int outch, int outh, int outw, int outChannelSize, const float* kernel, const float* bias, unsigned num_threads);
void conv7x7s2_neon(float *input, int inch, int h, int w, int inChannelSize, float *output, int outch, int outh, int outw, int outChannelSize, const float* kernel, const float* bias, unsigned num_threads);
void conv7x7s1_neon(float *input, int inch, int h, int w, int inChannelSize, float *output, int outch, int outh, int outw, int outChannelSize, const float* kernel, const float* bias, unsigned num_threads);
void conv5x5s2_neon(float *input, int inch, int h, int w, int inChannelSize, float *output, int outch, int outh, int outw, int outChannelSize, const float* kernel, const float* bias, unsigned num_threads);
void conv5x5s1_neon(float *input, int inch, int h, int w, int inChannelSize, float *output, int outch, int outh, int outw, int outChannelSize, const float* kernel, const float* bias, unsigned num_threads);
