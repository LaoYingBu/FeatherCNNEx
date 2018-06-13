#include "caffe.pb.h"
#include "feather_simple_generated.h"

#include <iostream>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <float.h>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include "common.h"

#if 1
#define PRINTF printf
#else
#define PRINTF
#endif

using namespace caffe;
using google::protobuf::io::FileInputStream;
using google::protobuf::Message;

class CaffeModelWeightsConvert
{
public:
    CaffeModelWeightsConvert(std::string caffe_prototxt_name, std::string caffe_model_name, std::string output_name);
    bool Convert();
    void SaveModelWeights(uint32_t fractions, float threshold);

private :
    bool ReadNetParam();

private :
    std::string caffe_prototxt_name;
    std::string caffe_model_name;
    std::string output_name;
    NetParameter caffe_weight;
    NetParameter caffe_prototxt;
};

CaffeModelWeightsConvert::CaffeModelWeightsConvert(std::string caffe_prototxt_name, std::string caffe_model_name, std::string output_name)
{
    this->caffe_prototxt_name = caffe_prototxt_name;
    this->caffe_model_name = caffe_model_name;
    this->output_name = output_name;
}

bool CaffeModelWeightsConvert::Convert()
{
    if (!ReadNetParam())
    {
        std::cerr << "Read net params fail!" << std::endl;
        return false;
    }

    return true;
}

bool CaffeModelWeightsConvert::ReadNetParam()
{
	{
		std::ifstream in(caffe_model_name.c_str());
		if (!in)
		{
			std::cerr << "read caffe model weights file " << caffe_model_name  <<" fail!" << std::endl;
			return false;
		}
		std::stringstream buffer;
		buffer << in.rdbuf();
		if (!caffe_weight.ParseFromString(std::string(buffer.str())))
		{
			std::cerr << "parse weights file " << caffe_model_name  <<" fail!" << std::endl;
			return false;
		}

		in.close();
	}

	{
		int fd = open(caffe_prototxt_name.c_str(), O_RDONLY);
		if (fd < 0)
		{
			std::cerr << "read caffe model prototxt " << caffe_prototxt_name  <<" fail!" << std::endl;
			return false;
		}

		FileInputStream* input = new FileInputStream(fd);
		bool success = google::protobuf::TextFormat::Parse(input, &caffe_prototxt);
		delete input;
		close(fd);
	}
    return true;
}

void CaffeModelWeightsConvert::SaveModelWeights(uint32_t frac, float threshold)
{
	std::string OutputLayerName;
	{
		uint32_t totalConvCnt = 0, dwConvCnt = 0, sgemmConvCnt = 0, winogradConvCnt = 0;
		float gminf, gmaxf, gabsminf;
		short gminS, gmaxS, gabsmaxS;
		int gFlag = 1;
		size_t input_layer_idx = -1;
		flatbuffers::FlatBufferBuilder fbb(204800);
		std::vector<flatbuffers::Offset<feather::LayerParameter>> layer_vec;
		std::vector<flatbuffers::Offset<flatbuffers::String>> 	input_name_vec;
		std::vector<int64_t>      								input_dim_vec;

		size_t input_num = caffe_prototxt.input_size();
		PRINTF("Input Num: %ld\n", input_num);
		const char *InputLayerName = "Input";
		if(input_num > 0)
		{
			assert(input_num == 1);
			for (int i = 0; i < input_num; ++i)
			{
				std::string input_name = caffe_prototxt.input(i);
				InputLayerName = input_name.c_str();
				PRINTF("Input name: %s\n", InputLayerName);
				input_name_vec.push_back(fbb.CreateString(input_name));
			}

			for(int i = 0; i < caffe_prototxt.input_shape_size(); ++i)
			{
				for(int j = 0; j < caffe_prototxt.input_shape(i).dim_size(); ++j)
				{
					size_t dim = caffe_prototxt.input_shape(i).dim(j);
					PRINTF("dim[%d]: %ld\n", j, dim);
					input_dim_vec.push_back((int64_t) dim);
				}
			}

			for(int i = 0; i < caffe_prototxt.input_dim_size(); ++i)
			{
				size_t dim = caffe_prototxt.input_dim(i);
				PRINTF("dim[%d]: %ld\n", i, dim);
				input_dim_vec.push_back(caffe_prototxt.input_dim(i));
			}
		}
		else
		{
			for (int i = 0; i != caffe_prototxt.layer_size(); ++i)
			{
				auto caffe_layer = caffe_prototxt.layer(i);
				std::string layer_type = caffe_layer.type();

				if(layer_type.compare("Input") == 0)
				{
					assert(caffe_layer.top_size() == 1);
					for(int j = 0; j < caffe_layer.top_size(); ++j)
					{
						InputLayerName = caffe_layer.top(j).c_str();
						PRINTF("Input name: %s\n", InputLayerName);
						input_name_vec.push_back(fbb.CreateString(caffe_layer.top(j)));
					}
					
					assert(caffe_layer.input_param().shape_size() == 1);
					for(int j = 0; j < caffe_layer.input_param().shape(0).dim_size(); ++j)
					{
						int64_t dim = caffe_layer.input_param().shape(0).dim(j);
						PRINTF("dim[%d]: %ld\n", j, dim);
						input_dim_vec.push_back(dim);
					}

					break;
				}
			}
		}

		//Create input parm & input layer
		auto input_param = feather::CreateInputParameterDirect(fbb,
				&input_name_vec,
				&input_dim_vec);
		auto input_layer_name = fbb.CreateString(InputLayerName);
		auto input_layer_type = fbb.CreateString("Input");
		feather::LayerParameterBuilder layer_builder(fbb);
		layer_builder.add_name(input_layer_name);
		layer_builder.add_type(input_layer_type);
		layer_builder.add_input_param(input_param);
		layer_vec.push_back(layer_builder.Finish());

		PRINTF("Layer Num: %d, Weight Num: %d\n", caffe_prototxt.layer_size(), caffe_weight.layer_size());

		std::vector<fix16_t> blob_data_vec_fix;
		std::vector<float> blob_data_vec;

		std::map<std::string, int> caffe_model_layer_map;
		for (int i = 0; i != caffe_weight.layer_size(); ++i)
		{
			std::string layer_name = caffe_weight.layer(i).name();
			caffe_model_layer_map[layer_name] = i;
			//printf("[%d] %s\n", i, layer_name.c_str());
		}

		std::map<std::string, std::string> inplace_blob_map;
		for (int i = 0; i != caffe_prototxt.layer_size(); ++i)
		{
			uint32_t fractions = 0;
			auto caffe_layer = caffe_prototxt.layer(i);
			std::string layer_name = caffe_layer.name();
			std::string layer_type = caffe_layer.type();

			if(layer_type.compare("Input")==0) continue;

			std::vector<std::string> bottom_vec;
			std::vector<std::string> top_vec;

			/*Bottom and top*/
			for(int j = 0; j < caffe_layer.bottom_size(); ++j)
			   	bottom_vec.push_back(caffe_layer.bottom(j));
			for(int j = 0; j < caffe_layer.top_size(); ++j)
			   	top_vec.push_back(caffe_layer.top(j));

			PRINTF("---------------------------------------\nLayer %d name %s type %s\nBottom: ", i, layer_name.c_str(), layer_type.c_str());

			/*Print bottom and tops*/
			for(int t = 0; t < bottom_vec.size(); ++t)
				PRINTF("%s ", bottom_vec[t].c_str());
			PRINTF("\nTop: ");
			for(int t = 0; t < top_vec.size(); ++t)
				PRINTF("%s ", top_vec[t].c_str());
			PRINTF("\n");

			/* change top blob name to layer name if bottom blob name eq top blob name */
			if(bottom_vec.size() > 0 && top_vec.size() > 0)
			{
				if(bottom_vec[0].compare(top_vec[0]) == 0)
				{
					assert(bottom_vec.size() == 1 && top_vec.size() == 1);

					std::string bottom_name = bottom_vec[0];
					if(inplace_blob_map.find(bottom_name) == inplace_blob_map.end())
						inplace_blob_map[bottom_name] = bottom_name;
					bottom_vec[0] = inplace_blob_map[bottom_name];
					PRINTF("[* CT] %s -> %s\n", top_vec[0].c_str(), layer_name.c_str());
					top_vec[0] = layer_name;
					inplace_blob_map[bottom_name] = layer_name;
				}
				else
				{
					for(int t = 0; t < bottom_vec.size(); ++t)
					{
						std::string bottom_name = bottom_vec[t];
						if(inplace_blob_map.find(bottom_name) != inplace_blob_map.end())
						{
							bottom_vec[t] = inplace_blob_map[bottom_name];
							PRINTF("[* CB] %s -> %s\n", bottom_name.c_str(), bottom_vec[t].c_str());
						}
					}
				}
			}

			PRINTF("New Bottom:");
			/* create flat buffer for bottom & top names  */
			std::vector<flatbuffers::Offset<flatbuffers::String>> bottom_fbstr_vec;
			for(int i = 0; i < bottom_vec.size(); ++i)
			{
				bottom_fbstr_vec.push_back(fbb.CreateString(bottom_vec[i]));
				PRINTF(" %s", bottom_vec[i].c_str());
			}
			auto bottom_fbvec = fbb.CreateVector<flatbuffers::Offset<flatbuffers::String>>(bottom_fbstr_vec);
			PRINTF("\nNew Top:");

			std::vector<flatbuffers::Offset<flatbuffers::String>> top_fbstr_vec;
			for(int i = 0; i < top_vec.size(); ++i)
			{
				top_fbstr_vec.push_back(fbb.CreateString(top_vec[i]));
				OutputLayerName = top_vec[i];
				PRINTF(" %s", top_vec[i].c_str());
			}
			auto top_fbvec = fbb.CreateVector<flatbuffers::Offset<flatbuffers::String>>(top_fbstr_vec);
			PRINTF("\n");

			// First step, only 1x1 conv sgemm used fix16
			if (layer_type.compare("Convolution")==0)
			{
				auto caffe_conv_param = caffe_layer.convolution_param();
				if (1 == caffe_conv_param.kernel_size(0))
					fractions = frac;
			}
			else if (layer_type.compare("ConvolutionDepthwise")==0)
			{
			    uint32_t step_h = 1, step_w = 1;

				auto caffe_conv_param = caffe_layer.convolution_param();
				if(caffe_conv_param.stride_size() == 1){
					step_h = step_w = caffe_conv_param.stride(0);
				}
				else if(caffe_conv_param.stride_size() == 2){
					step_h = caffe_conv_param.stride(0);
					step_w = caffe_conv_param.stride(1);
				}
				else if(caffe_conv_param.stride_size() == 0)
				{
					step_h = step_w = 1;
				}
				else
				{
					PRINTF("\nERR: code should not reach here as wrong stride size\n");
					exit(-1);
				}

				if (((1 == step_h) && (1 == step_w))
					|| ((2 == step_h) && (2 == step_w))
					)
					fractions = frac;
			}

			/* Blobs */
			auto caffe_model_layer = caffe_weight.layer(caffe_model_layer_map[layer_name]);
			PRINTF("Blob num (%s): %d\n", layer_type.c_str(), caffe_model_layer.blobs_size());
			std::vector<flatbuffers::Offset<feather::BlobProto> > blob_vec;

			for (int j = 0; j != caffe_model_layer.blobs_size(); ++j)
			{
				uint32_t zeroCnt = 0;
				float minf, maxf, absminf;
				short minS, maxS, absmaxS, absminS;

				auto caffe_blob = caffe_model_layer.blobs(j);
				int dim_len = caffe_blob.shape().dim_size();

				PRINTF("	Blob[%02d], dim_len: %02d, data size: %d\n", j, dim_len, caffe_blob.data_size());

				/* push blob data to fbb */
				for(int k = 0; k != caffe_blob.data_size(); ++k)
				{
					float data = caffe_blob.data(k);
					/* only weight blob of Conv layer do fix16 change (bias ignore) */
					if ((0 == j) && ((layer_type.compare("Convolution")==0) || (layer_type.compare("ConvolutionDepthwise")==0)))
					{
						fix16_t fix_data = FLOAT2FIX((fix16_t), fractions, data);
						blob_data_vec_fix.push_back(fix_data);
						blob_data_vec.push_back(data);

						if (0 == k) { minf = maxf = data; minS = maxS = fix_data; }
						minf = MIN(minf, data);maxf = MAX(maxf, data);
						absminf = MIN(fabs(minf), fabs(maxf));

						minS = MIN(minS, fix_data);maxS = MAX(maxS, fix_data);
						absmaxS = MAX(abs(minS), abs(maxS));
						absminS = MIN(abs(minS), abs(maxS));
					}
					else
						blob_data_vec.push_back(data);
				}

				if ((0 == j) && ((layer_type.compare("Convolution")==0) || (layer_type.compare("ConvolutionDepthwise")==0)))
				{
					if (gFlag)
					{
						gminS = minS;
						gmaxS = maxS;
						gabsmaxS = absmaxS;

						gminf = minf;
						gmaxf = maxf;
						gabsminf = absminf;

						gFlag = 0;
					}
					else
					{
						gminS = MIN(minS, gminS);
						gmaxS = MAX(maxS, gmaxS);
						gabsmaxS = MAX(absmaxS, gabsmaxS);

						gminf = MIN(minf, gminf);
						gmaxf = MAX(maxf, gmaxf);
						gabsminf = MIN(absminf, gabsminf);
					}

					printf("	[%f, %f] [%f, %f] [%f]\n", minf, maxf, gminf, gmaxf, gabsminf);
					if (0 != fractions)
					{
						PRINTF("	[%d %d] [%d %d] [%d %d] [%d] %d\n", minS, maxS, absminS, absmaxS, gminS, gmaxS, gabsmaxS, 1<<fractions);
						for(int k = 0; k != caffe_blob.data_size(); ++k)
							if (abs(blob_data_vec_fix[k]) < (absminS*threshold)) zeroCnt++;
						auto caffe_conv_param = caffe_layer.convolution_param();
						if (caffe_conv_param.has_kernel_h() || caffe_conv_param.has_kernel_w())
							printf("[%-20s] [%-40s] [%dX%d] Sparse Info: %06.3f%% [%05d %05d] %f\n", layer_type.c_str(), layer_name.c_str(),  caffe_conv_param.kernel_h(), caffe_conv_param.kernel_w(), (zeroCnt*100.0f)/caffe_blob.data_size(), absminS, absmaxS, threshold);
						else
							printf("[%-20s] [%-40s] [%dX%d] Sparse Info: %06.3f%% [%05d %05d] %f\n", layer_type.c_str(), layer_name.c_str(),  caffe_conv_param.kernel_size(0), caffe_conv_param.kernel_size(0), (zeroCnt*100.0f)/caffe_blob.data_size(), absminS, absmaxS, threshold);
					}
				}

				flatbuffers::Offset<flatbuffers::Vector<short> > blob_data_fbvec_fix;
				flatbuffers::Offset<flatbuffers::Vector<float> > blob_data_fbvec;
				if ((0 == j) && (0 != fractions) && ((layer_type.compare("Convolution")==0) || (layer_type.compare("ConvolutionDepthwise")==0)))
				{
					blob_data_fbvec_fix = fbb.CreateVector<fix16_t>(blob_data_vec_fix);
					PRINTF("	Blob Fix %d\n", fractions);
				}
				else
					blob_data_fbvec = fbb.CreateVector<float>(blob_data_vec);
				feather::BlobProtoBuilder blob_builder(fbb);
				if ((0 == j) && (0 != fractions) && ((layer_type.compare("Convolution")==0) || (layer_type.compare("ConvolutionDepthwise")==0)))
					blob_builder.add_data_fix(blob_data_fbvec_fix);
				else
					blob_builder.add_data(blob_data_fbvec);

				/* push blob dim info to fbb */
				size_t num, channels, height, width;
				if(dim_len == 0)
				{
					num = caffe_blob.num();
					channels = caffe_blob.channels();
					height = caffe_blob.height();
					width = caffe_blob.width();
					PRINTF("	blob shape change from (%lu %lu %lu %lu)", num, channels, height, width);
					if(num == 1 && channels == 1 && height == 1 && width > 1)
					{
						num = width;
						width = 1;
					}
					if(num == 1 && channels == 1 && height > 1 && width > 1)
					{
						num = height;
						channels = width;
						height = 1;
						width = 1;
					}
					PRINTF("to (%lu %lu %lu %lu)\n", num, channels, height, width);
				}
				else
				{
					if(caffe_blob.shape().dim_size() == 4)
					{
						num = caffe_blob.shape().dim(0);
						channels = caffe_blob.shape().dim(1);
						height = caffe_blob.shape().dim(2);
						width = caffe_blob.shape().dim(3);
					}
					else if(caffe_blob.shape().dim_size() == 1)
					{
						num = caffe_blob.shape().dim(0);
						channels = 1;
						height = 1;
						width = 1;
					}
					else if(caffe_blob.shape().dim_size() == 2)
					{
						num = caffe_blob.shape().dim(0);
						channels = caffe_blob.shape().dim(1);
						height = 1;
						width = 1;
					}
					else if(caffe_blob.shape().dim_size() == 3)
					{
						num = 1;
						channels = caffe_blob.shape().dim(0);
						height = caffe_blob.shape().dim(1);
						width = caffe_blob.shape().dim(2);
					}
					else
						PRINTF("Unsupported dimension with dim size %d\n", caffe_blob.shape().dim_size());
				}

				PRINTF("	[%ld, %ld, %ld, %ld, Fractions:", num, channels, height, width);

				if ((0 == j) && (0 != fractions) && ((layer_type.compare("Convolution")==0) || (layer_type.compare("ConvolutionDepthwise")==0)))
				{
					blob_builder.add_fractions(fractions);
					PRINTF(" %d]\n", fractions);
				}
				else
				{
					blob_builder.add_fractions(0);
					PRINTF(" 0]\n");
				}
				blob_builder.add_num(num);
				blob_builder.add_channels(channels);
				blob_builder.add_height(height);
				blob_builder.add_width(width);
				blob_vec.push_back(blob_builder.Finish());
				blob_data_vec_fix.clear();
				blob_data_vec.clear();
			}
			auto blobs_fbvec = fbb.CreateVector<flatbuffers::Offset<feather::BlobProto> >(blob_vec);
			blob_vec.clear();
			/*--------------------------blob data & dim info add end-----------------------------------*/

			/*------------------------------------Params-----------------------------------------------*/
			flatbuffers::Offset<feather::ConvolutionParameter> conv_param;
			flatbuffers::Offset<feather::LRNParameter> lrn_param;
			flatbuffers::Offset<feather::PoolingParameter> pooling_param;
			flatbuffers::Offset<feather::BatchNormParameter> bn_param;
			flatbuffers::Offset<feather::ScaleParameter> scale_param;
			flatbuffers::Offset<feather::EltwiseParameter> eltwise_param;
			flatbuffers::Offset<feather::InnerProductParameter> inner_product_param;
			flatbuffers::Offset<feather::PReLUParameter> prelu_param;
			flatbuffers::Offset<feather::DropoutParameter> dropout_param;
			PRINTF("Layer param:\n");
			if((layer_type.compare("Convolution")==0) || (layer_type.compare("ConvolutionDepthwise")==0)){
				uint32_t k_w, k_h, stride_h, stride_w, pad_h, pad_w;
				totalConvCnt++;
				PRINTF("+ %s\n", layer_type.c_str());
				auto caffe_conv_param = caffe_layer.convolution_param();
				feather::ConvolutionParameterBuilder conv_param_builder(fbb);
				PRINTF("+ bias term %d\n", caffe_conv_param.bias_term());
				conv_param_builder.add_bias_term(caffe_conv_param.bias_term());
				if(caffe_conv_param.kernel_size_size() == 1)
				{
					k_w = k_h = caffe_conv_param.kernel_size(0);
					conv_param_builder.add_kernel_h(caffe_conv_param.kernel_size(0));
					conv_param_builder.add_kernel_w(caffe_conv_param.kernel_size(0));
				}
				else if(caffe_conv_param.kernel_size_size() == 2)
				{
					conv_param_builder.add_kernel_h(caffe_conv_param.kernel_size(0));
					conv_param_builder.add_kernel_w(caffe_conv_param.kernel_size(1));
					k_h = caffe_conv_param.kernel_size(0);
					k_w = caffe_conv_param.kernel_size(1);
				}
				else
				{
					if (caffe_conv_param.has_kernel_h() && caffe_conv_param.has_kernel_w())
					{
						conv_param_builder.add_kernel_h(caffe_conv_param.kernel_h());
						conv_param_builder.add_kernel_w(caffe_conv_param.kernel_w());
						k_h = caffe_conv_param.kernel_h();
						k_w = caffe_conv_param.kernel_w();
					}
					else
					{
						PRINTF("\nERR: code should not reach here as wrong kernel size\n");
						exit(-1);
					}
				}

				PRINTF("+ k [%d %d]\n", k_h, k_w);

				if(caffe_conv_param.stride_size() == 1){
					conv_param_builder.add_stride_h(caffe_conv_param.stride(0));
					conv_param_builder.add_stride_w(caffe_conv_param.stride(0));
					stride_h = stride_w = caffe_conv_param.stride(0);
				}
				else if(caffe_conv_param.stride_size() == 2){
					conv_param_builder.add_stride_h(caffe_conv_param.stride(0));
					conv_param_builder.add_stride_w(caffe_conv_param.stride(1));
					stride_h = caffe_conv_param.stride(0);
					stride_w = caffe_conv_param.stride(1);
				}
				else if(caffe_conv_param.stride_size() == 0)
				{
					conv_param_builder.add_stride_h(1);
					conv_param_builder.add_stride_w(1);
					stride_h = stride_w = 1;
				}
				else
				{
					PRINTF("\nERR: code should not reach here as wrong stride size\n");
					exit(-1);
				}

				PRINTF("+ stride [%d %d]\n", stride_h, stride_w);

				if(caffe_conv_param.pad_size() == 1)
				{
					conv_param_builder.add_pad_h(caffe_conv_param.pad(0));
					conv_param_builder.add_pad_w(caffe_conv_param.pad(0));
					pad_h = pad_w = caffe_conv_param.pad(0);
				}
				else if(caffe_conv_param.pad_size() == 2)
				{
					conv_param_builder.add_pad_h(caffe_conv_param.pad(0));
					conv_param_builder.add_pad_w(caffe_conv_param.pad(1));
					pad_h = caffe_conv_param.pad(0);
					pad_w = caffe_conv_param.pad(1);
				}
				else if(caffe_conv_param.pad_size() == 0 && caffe_conv_param.has_pad_h() && caffe_conv_param.has_pad_w())
				{
					conv_param_builder.add_pad_h(caffe_conv_param.pad_h());
					conv_param_builder.add_pad_w(caffe_conv_param.pad_w());
					pad_h = caffe_conv_param.pad_h();
					pad_w = caffe_conv_param.pad_w();
				}
				else
				{
					conv_param_builder.add_pad_h(0);
					conv_param_builder.add_pad_w(0);
					pad_h = pad_w = 0;
				}

				PRINTF("+ pad [%d %d]\n", pad_h, pad_w);

				conv_param_builder.add_fractions(fractions);
				PRINTF("+ fractions %u\n", fractions);

				if (layer_type.compare("ConvolutionDepthwise")==0)
					conv_param_builder.add_group(caffe_conv_param.num_output());
				else
					conv_param_builder.add_group(caffe_conv_param.group());
				PRINTF("+ num_output %u\n", caffe_conv_param.num_output());

				if (layer_type.compare("ConvolutionDepthwise")==0)
				{
					dwConvCnt++;
					PRINTF("+ group %d\n", caffe_conv_param.num_output());
				}
				else
				{
					if (3 != k_h || 3 != k_w)
						sgemmConvCnt++;
					PRINTF("+ group %d\n", caffe_conv_param.group());
				}

				conv_param = conv_param_builder.Finish();
			}
			else if(layer_type.compare("LRN") == 0)
			{
				auto caffe_lrn_param = caffe_layer.lrn_param();
				size_t local_size = caffe_lrn_param.local_size();
				float alpha = caffe_lrn_param.alpha();
				float beta = caffe_lrn_param.beta();
				float k = caffe_lrn_param.k();
				PRINTF("+ local_size %ld alpha %f beta %f k %f\n", local_size, alpha, beta, k);
				feather::LRNParameterBuilder lrn_param_builder(fbb);
				lrn_param_builder.add_local_size(local_size);
				lrn_param_builder.add_alpha(alpha);
				lrn_param_builder.add_beta(beta);
				lrn_param_builder.add_k(k);
				switch(caffe_lrn_param.norm_region())
				{
					case caffe::LRNParameter_NormRegion_ACROSS_CHANNELS:
						PRINTF("+ Across channels\n");
						lrn_param_builder.add_norm_region(feather::LRNParameter_::NormRegion_ACROSS_CHANNELS);	
						break;
					case caffe::LRNParameter_NormRegion_WITHIN_CHANNEL:
						PRINTF("+ Within channels\n");
						lrn_param_builder.add_norm_region(feather::LRNParameter_::NormRegion_WITHIN_CHANNEL);	
						break;
					default:
						PRINTF("Unknown LRN method\n");
						exit(-1);
				}
				lrn_param = lrn_param_builder.Finish();	
			}
			else if(layer_type.compare("Pooling")==0)
			{
				auto caffe_pooling_param = caffe_layer.pooling_param();
				feather::PoolingParameterBuilder pooling_param_builder(fbb);
				switch(caffe_pooling_param.pool()){
					case caffe::PoolingParameter_PoolMethod_MAX:
						pooling_param_builder.add_pool(feather::PoolingParameter_::PoolMethod_MAX_);
						break;
					case caffe::PoolingParameter_PoolMethod_AVE:
						pooling_param_builder.add_pool(feather::PoolingParameter_::PoolMethod_AVE);
						break;
					case caffe::PoolingParameter_PoolMethod_STOCHASTIC:
						pooling_param_builder.add_pool(feather::PoolingParameter_::PoolMethod_STOCHASTIC);
						break;
					default:
						//error handling
						;
				}
				if(caffe_pooling_param.has_pad())
				{
					pooling_param_builder.add_pad_h(caffe_pooling_param.pad());
					pooling_param_builder.add_pad_w(caffe_pooling_param.pad());
				}
				else
				{
					pooling_param_builder.add_pad_h(caffe_pooling_param.pad_h());
					pooling_param_builder.add_pad_w(caffe_pooling_param.pad_w());
				}
				if(caffe_pooling_param.has_kernel_size())
				{
					pooling_param_builder.add_kernel_h(caffe_pooling_param.kernel_size());
					pooling_param_builder.add_kernel_w(caffe_pooling_param.kernel_size());
				}
				else
				{
					pooling_param_builder.add_kernel_h(caffe_pooling_param.kernel_h());
					pooling_param_builder.add_kernel_w(caffe_pooling_param.kernel_w());
				}
				//pooling_param_builder.add_kernel_size(caffe_pooling_param.kernel_size());
				if(caffe_pooling_param.has_stride())
				{
					pooling_param_builder.add_stride_h(caffe_pooling_param.stride());
					pooling_param_builder.add_stride_w(caffe_pooling_param.stride());
				}
				else
				{
					pooling_param_builder.add_stride_h(caffe_pooling_param.stride_h());
					pooling_param_builder.add_stride_w(caffe_pooling_param.stride_w());
				}
				pooling_param_builder.add_global_pooling(caffe_pooling_param.global_pooling());
				pooling_param = pooling_param_builder.Finish();
			}
			else if(layer_type.compare("InnerProduct")==0)
			{
				auto caffe_inner_product_param = caffe_layer.inner_product_param();
				feather::InnerProductParameterBuilder inner_product_param_builder(fbb);
				inner_product_param_builder.add_bias_term(caffe_inner_product_param.bias_term());
				inner_product_param = inner_product_param_builder.Finish();	
			}
			else if(layer_type.compare("BatchNorm")==0)
			{
				//Do nothing
			}
			else if(layer_type.compare("Softmax")==0)
			{

			}
			else if(layer_type.compare("Scale")==0)
			{
				auto caffe_scale_param = caffe_layer.scale_param();
				PRINTF("+ Scale param %d\n", caffe_scale_param.bias_term());
				feather::ScaleParameterBuilder scale_param_builder(fbb);
				scale_param_builder.add_bias_term(caffe_scale_param.bias_term());
				scale_param = scale_param_builder.Finish();
			}
			else if(layer_type.compare("Eltwise")==0)
			{
				auto caffe_eltwise_param = caffe_layer.eltwise_param();
				auto op = caffe_eltwise_param.operation();
				feather::EltwiseParameter_::EltwiseOp feather_op;
				switch(op)
				{
					case EltwiseParameter_EltwiseOp_PROD:
						PRINTF("+ PROD op\n");
						feather_op = feather::EltwiseParameter_::EltwiseOp_PROD;
						break;
					case EltwiseParameter_EltwiseOp_SUM:
						PRINTF("+ SUM op\n");
						feather_op = feather::EltwiseParameter_::EltwiseOp_SUM;
						break;
					case EltwiseParameter_EltwiseOp_MAX:
						PRINTF("+ MAX op\n");
						feather_op = feather::EltwiseParameter_::EltwiseOp_MAX;
						break;
					defalut:
						PRINTF("Unknown eltwise parameter.\n");
				}
				std::vector<float> coeff_vec;
				for(int i = 0; i < caffe_eltwise_param.coeff_size(); ++i)
				{
					coeff_vec.push_back(caffe_eltwise_param.coeff(i));	
				}
				PRINTF("+ Loaded coeff size %ld\n", coeff_vec.size());
				eltwise_param = feather::CreateEltwiseParameterDirect(fbb, feather_op, &coeff_vec);
			}
			else if(layer_type.compare("ReLU")==0)
			{
				//Do nothing
			}
			else if(layer_type.compare("PReLU")==0)
			{
			
			}
			else if(layer_type.compare("Dropout")==0)
			{
				float scale = 1.0f;
				auto caffe_dropout_param = caffe_layer.dropout_param();

				scale = caffe_dropout_param.dropout_ratio();
				PRINTF("+ dropout scale: %f\n", scale);

				feather::DropoutParameterBuilder dropout_param_builder(fbb);
				dropout_param_builder.add_dropout_ratio(scale);
				dropout_param = dropout_param_builder.Finish();	
			}

			auto layer_name_fbb = fbb.CreateString(layer_name);
			flatbuffers::Offset<flatbuffers::String> layer_type_fbb;
			if((layer_type.compare("Convolution")==0) || (layer_type.compare("ConvolutionDepthwise")==0))
				layer_type_fbb = fbb.CreateString("Convolution");
			else
				layer_type_fbb = fbb.CreateString(layer_type);
			feather::LayerParameterBuilder layer_builder(fbb);
			layer_builder.add_bottom(bottom_fbvec);
			layer_builder.add_top(top_fbvec);
			layer_builder.add_blobs(blobs_fbvec);
			layer_builder.add_name(layer_name_fbb);
			layer_builder.add_type(layer_type_fbb);
			if((layer_type.compare("Convolution")==0) || (layer_type.compare("ConvolutionDepthwise")==0))
				layer_builder.add_convolution_param(conv_param);
			else if(layer_type.compare("LRN")==0)
				layer_builder.add_lrn_param(lrn_param);
			else if(layer_type.compare("Pooling")==0)
				layer_builder.add_pooling_param(pooling_param);
			else if(layer_type.compare("InnerProduct")==0)
				layer_builder.add_inner_product_param(inner_product_param);
			else if(layer_type.compare("Scale")==0)
				layer_builder.add_scale_param(scale_param);
			else if(layer_type.compare("Eltwise")==0)
				layer_builder.add_eltwise_param(eltwise_param);
			else if(layer_type.compare("PReLU")==0)
				layer_builder.add_prelu_param(prelu_param);
			else if(layer_type.compare("Dropout")==0)
				layer_builder.add_dropout_param(dropout_param);

			layer_vec.push_back(layer_builder.Finish());
		}

		printf("\nTotal Conv: %02d, Sgemm Conv: %02d, DW Conv: %02d, winograd Conv: %02d\n", totalConvCnt, sgemmConvCnt, dwConvCnt, totalConvCnt - sgemmConvCnt -dwConvCnt);

		auto layer_fbvec = fbb.CreateVector<flatbuffers::Offset<feather::LayerParameter>>(layer_vec);
		auto name_fbb = fbb.CreateString(caffe_prototxt.name());
		feather::NetParameterBuilder net_builder(fbb);
		net_builder.add_layer(layer_fbvec);
		net_builder.add_name(name_fbb);
		auto net = net_builder.Finish();
		fbb.Finish(net);
		uint8_t* net_buffer_pointer = fbb.GetBufferPointer();
		size_t size = fbb.GetSize();

		std::stringstream tmp; tmp<<frac;
		std::string outfile = output_name+"_"+OutputLayerName+"_"+tmp.str()+".feathermodel";
		FILE *netfp = NULL;
		netfp = fopen(outfile.c_str(), "wb");
		fwrite(net_buffer_pointer, sizeof(uint8_t), size, netfp);
		fclose(netfp);
		printf("\nconvert ok!!!!!!\n");
		printf("Model file: %s, size: %ld\n\n", outfile.c_str(), size);
	}
}

int main(int argc, char *argv[])
{
	uint32_t fractions = 0;
	float threshold = 0.02f;
	if (argc < 3 || argc > 6)
	{
		printf("Usage: ./caffe_model_convert $1(caffe_prototxt) $2(caffe_model_name) [$3(output_model_name_prefix)] [$4(fractions)] [$5(threshold)]\n");
		return -1;
	}
	std::string caffe_prototxt_name = argv[1];
	std::string caffe_model_name = argv[2];
	std::string output_model_name = "out";
	if (argc > 3) output_model_name = (argv[3]);
	if (argc > 4) fractions = atoi(argv[4]);
	if (argc > 5) threshold = atof(argv[5]);

	printf("%s caffe proto: %s caffe model: %s featherCNN: %s fractions:%d threshold:%.3f\n", argv[0], argv[1], argv[2], output_model_name.c_str(), fractions, threshold);
	CaffeModelWeightsConvert convert(caffe_prototxt_name, caffe_model_name, output_model_name);
	if (false == convert.Convert())
	{
		printf("Read file failed\n");
		return -2;
	}
	convert.SaveModelWeights(fractions, threshold);
	return 0;
}
