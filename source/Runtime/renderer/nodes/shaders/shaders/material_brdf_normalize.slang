// Normalize Sample Buffer to Texture
// Converts atomic accumulation to normalized float texture

RWStructuredBuffer<uint> sample_buffer;  // Input: [pdf_sum, count] pairs stored as uint (float bits)
RWTexture2D<float4> sample_output;       // Output: normalized texture

struct NormalizeConstants {
    uint resolution;
    uint num_samples;
};

ConstantBuffer<NormalizeConstants> normalize_params;

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadID.xy;
    uint resolution = normalize_params.resolution;
    
    if (any(pixel >= resolution)) {
        return;
    }
    
    uint pixel_index = pixel.y * resolution + pixel.x;
    
    // Read accumulated values (stored as uint, convert back to float)
    float sample_count = asfloat(sample_buffer[pixel_index * 2 + 0]);
    float total_samples_marker = asfloat(sample_buffer[pixel_index * 2 + 1]);
    
    // Normalize to probability density: frequency / total_samples
    // So sum of all pixels = 1.0
    float normalized_frequency = sample_count / float(normalize_params.num_samples);
    
    // Output normalized frequency (probability density)
    // RGB = normalized frequency, A = raw count for debugging
    sample_output[pixel] = float4(normalized_frequency, normalized_frequency, normalized_frequency, sample_count);
}
