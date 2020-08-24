#pragma once

#include <cstdlib>
#include "OpenGP/GL/Application.h"

using namespace OpenGP;

// Parameterised linear interpolation
inline float lerp(float x, float y, float t) {
    return (y - x) * t + x;
}

inline float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

// Random number generation for basis
inline float rand01() {
    return ((float) std::rand())/((float) RAND_MAX);
}

float* perlin2D(const int width, const int height, const int period=64);

// Generate a heightmap using either fractional Brownian motion or hybrid multifractals
// TODO: Read user's choice from stdin
R32FTexture* generateNoise() {

    // Precompute perlin noise on a 2D grid
    const int width = 512;
    const int height = 512;
    float *perlin_data = perlin2D(width, height, 128);

    // Initialise basis parameters
    float H = 0.8f;
    float lacunarity = 2.0f;
    float offset = 0.1f;
    const int octaves = 4;

    // Initialize to 0s
    float *noise_data = new float[width*height];
    for (int i = 0; i < width; ++ i) {
        for (int j = 0; j < height; ++ j) {
            noise_data[i + j * height] = 0;
        }
    }

    // Precompute exponent array
    float *exponent_array = new float[octaves];
    float frequency = 1.0f;
    for (int i = 0; i < octaves; ++i) {
        exponent_array[i] = std::pow(frequency, -H);
        frequency *= lacunarity;
    }

    for (int i = 0; i < width; ++i) {
        for (int j = 0; j < height; ++j) {
            int I = i;
            int J = j;

            for(int k = 0; k < octaves; ++k) {
                int p = (I % width) + (J % height) * height;

                /*
                // Use Hybrid Multifractal as noise generating basis
                float perlinValue = (1 - abs(perlin_data[p] + offset)) * exponent_array[0];
                float weight = perlinValue;
                weight = (weight < 1.0f) ? 1.0f : weight;

                // Generate Perlin value
                float perlinR = (1 - abs(perlin_data[p])) * exponent_array[k];

                // Add weighted Perlin value to Perlin
                perlinValue += (weight * perlinR);

                // Adjust weighting value
                weight *= perlinR;

                // Generate noise
                int noiseIndex = i + j * height;
                noise_data[noiseIndex] = perlinValue;
                */

                // Use Fractional Brownian motion as noise generating basis
                float perlinValue = perlin_data[p];

                // Generate noise
                int n = i + j * height;
                float noise = offset + perlinValue * exponent_array[k];
                noise_data[n] += (noise);

                // Point to sample at next octave
                I *= (int) lacunarity;
                J *= (int) lacunarity;
            }
        }
    }
    // Load heightmap
    R32FTexture* _tex = new R32FTexture();
    _tex->upload_raw(width, height, noise_data);

    // Clear arrays
    delete[] perlin_data;
    delete[] noise_data;
    delete[] exponent_array;

    return _tex;
}

float* perlin2D(const int width, const int height, const int period) {

    // Precompute random gradients
    float *gradients = new float[width * height * 2];
    auto sample_gradient = [&](int i, int j) {
        float x = gradients[2 * (i + j * height)];
        float y = gradients[2 * (i + j * height) + 1];
        return Vec2(x, y);
    };

    for (int i = 0; i < width; ++ i) {
        for (int j = 0; j < height; ++ j) {
            float angle = rand01();
            gradients[2*(i+j*height)] = cos(2 * angle * M_PI);
            gradients[2*(i+j*height)+1] = sin(2 * angle * M_PI);
        }
    }

    // Perlin Noise parameters
    float frequency = 1.0f / period;

    float *perlin_data = new float[width*height];
    for (int i = 0; i < width; ++ i) {
        for (int j = 0; j < height; ++ j) {

            // Integer coordinates of corners
            int left = (i / period) * period;
            int right = (left + period) % width;
            int top = (j / period) * period;
            int bottom = (top + period) % height;

            // Local coordinates [0,1] within each block
            float dx = (i - left) * frequency;
            float dy = (j - top) * frequency;

            // Fetch random vectors at corners
            Vec2 topleft = sample_gradient(left, top);
            Vec2 topright = sample_gradient(right, top);
            Vec2 bottomleft = sample_gradient(left, bottom);
            Vec2 bottomright = sample_gradient(right, bottom);

            // Vector from each corner to pixel center
            Vec2 a(dx, -dy); // topleft
            Vec2 b(dx-1, -dy); // topright
            Vec2 c(dx, 1-dy); // bottomleft
            Vec2 d(dx-1, 1-dy); // bottomright

            // Initialise scalars for each corner
            float s = a.dot(topleft);
            float t = b.dot(topright);
            float u = c.dot(bottomleft);
            float v = d.dot(bottomright);

            // Interpolate along 'x' axis
            float st = lerp(s, t, fade(dx));
            float uv = lerp(u, v, fade(dx));

            // Interpolate along 'y' axis
            float noise = lerp(st, uv, fade(dy));

            // Populate array
            perlin_data[i + j * height] = noise;
        }
    }

    // Clear arrays
    delete[] gradients;
    return perlin_data;
}
