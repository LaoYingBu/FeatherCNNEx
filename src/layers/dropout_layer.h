//Tencent is pleased to support the open source community by making FeatherCNN available.

//Copyright (C) 2018 THL A29 Limited, a Tencent company. All rights reserved.

//Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
//in compliance with the License. You may obtain a copy of the License at
//
//https://opensource.org/licenses/BSD-3-Clause
//
//Unless required by applicable law or agreed to in writing, software distributed
//under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//CONDITIONS OF ANY KIND, either express or implied. See the License for the
//specific language governing permissions and limitations under the License.

#pragma once

#include "../feather_simple_generated.h"
#include "../layer.h"

namespace feather
{
class DropoutLayer : public Layer
{
public:
    DropoutLayer(const LayerParameter *layer_param, const RuntimeParameter<float>* rt_param)
        : Layer(layer_param, rt_param)
    {
        const DropoutParameter *dropout_param = layer_param->dropout_param();
        scale = 1.0 - dropout_param->dropout_ratio();
    }

    int Forward()
    {
        int size = w * h;
        //printf("[DROPOUT] bottom:%s top:%s c:%d h:%d w:%d [%f %f %f %f]\n", _bottom[0].c_str(), _top[0].c_str(), c,h,w, input[0], input[1], input[2], input[3]);

        #pragma omp parallel for
        for (int q=0; q<c; q++)
        {
            const float* inPtr = input + q*size;
            float* outPtr = output + q*size;

            for (int i=0; i<size; i++)
            {
                outPtr[i] = inPtr[i] * scale;
            }
        }

        return 0;
    }

    int Init(float *ginput, float *goutput)
    {
        if ((NULL != ginput) && (NULL != ginput))
        {
            ((Blob<float> *)_bottom_blobs[_bottom[0]])->setData(ginput);
            ((Blob<float> *)_top_blobs[_top[0]])->setData(goutput);
        }

        input = _bottom_blobs[_bottom[0]]->data();
        output = _top_blobs[_top[0]]->data();
        n = _bottom_blobs[_bottom[0]]->num();
        c = _bottom_blobs[_bottom[0]]->channels();
        w = _bottom_blobs[_bottom[0]]->width();
        h = _bottom_blobs[_bottom[0]]->height();

        return 0;
    }
protected:
    float scale;
    int n, c, w, h;
};
};
