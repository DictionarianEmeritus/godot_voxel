#ifndef RGBLIGHT_H
#define RGBLIGHT_H

#include <array>
#include <algorithm>

struct alignas(1) RGBLight {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    void dim(uint8_t amount, uint8_t minAmount) {
        r = (r - amount > minAmount) ? r - amount : minAmount;
        g = (g - amount > minAmount) ? g - amount : minAmount;
        b = (b - amount > minAmount) ? b - amount : minAmount;
    }

    void blendMax(const RGBLight& other) {
        r = (r > other.r) ? r : other.r;
        g = (g > other.g) ? g : other.g;
        b = (b > other.b) ? b : other.b;
    }

    uint8_t maxComponent() const {
        uint8_t max1 = (r > g) ? r : g;
        return (max1 > b) ? max1 : b;
    }
};

inline RGBLight sample_point_trilinear(int x, int y, int z, std::array<RGBLight, 20 * 20 * 20> lightData);

// Don't return 255, 255, 255 here, since that has a special significance inside the algorithm and behaves as sunlight
// 0, 0, 0 means not emissive
inline RGBLight getEmissiveColor(int typeId) {
    if (typeId <= 16 || typeId > 32) {
        return RGBLight{0, 0, 0};
    }

    const RGBLight emissiveColors[16] = {
        RGBLight{239, 239, 239},    // 1
        RGBLight{175, 15, 15},
        RGBLight{15, 175, 15},
        RGBLight{175, 95, 15},
        RGBLight{15, 15, 175},
        RGBLight{175, 15, 175},
        RGBLight{15, 175, 175},
        RGBLight{175, 175, 175},    // 8
        RGBLight{95, 95, 95},
        RGBLight{239, 95, 95},
        RGBLight{95, 239, 95},
        RGBLight{239, 239, 95},
        RGBLight{95, 95, 239},
        RGBLight{239, 95, 239},
        RGBLight{95, 239, 239},
        RGBLight{15, 15, 15},       // 16
    };

    // const RGBLight emissiveColors[16] = {
    //     RGBLight{239, 239, 239},    // 1
    //     RGBLight{95, 15, 15},
    //     RGBLight{15, 95, 15},
    //     RGBLight{95, 63, 15},
    //     RGBLight{15, 15, 95},
    //     RGBLight{95, 15, 95},
    //     RGBLight{15, 95, 95},
    //     RGBLight{95, 95, 95},       // 8
    //     RGBLight{63, 63, 63},
    //     RGBLight{127, 63, 63},
    //     RGBLight{63, 127, 63},
    //     RGBLight{127, 127, 63},
    //     RGBLight{63, 63, 127},
    //     RGBLight{127, 63, 127},
    //     RGBLight{63, 127, 127},
    //     RGBLight{15, 15, 15},       // 16
    // };

    return emissiveColors[typeId - 17];
}

inline unsigned int index3D(unsigned int x, unsigned int y, unsigned int z, unsigned int size = 18) {
    return (x * size + y) * size + z;
}

inline void printLights(RGBLight* lights) {
    // for (int i = 0; i < 18; ++i) {
    for (int i = 0; i < 1; ++i) {
        printf("\n");
        for (int j = 0; j < 18; ++j) {
            for (int k = 0; k < 18; ++k) {
                RGBLight value = lights[index3D(j, i, k)];
                printf("%d,", value.r);
            }
            printf("\n");
        }
    }
}

inline RGBLight sample_lighting_flat(zylann::Vector3f position_world, zylann::Vector3f vertex_position, zylann::Vector3f side_normal, std::array<RGBLight, 20*20*20> lightData, int lightMinimum, bool useShadowTrick, int shadowPenalty) {
    position_world += side_normal;
    // position_world += zylann::Vector3f(1.0, 1.0, 1.0);
    position_world += zylann::Vector3f(2.0, 2.0, 2.0);
	float x = position_world.x;
	float y = position_world.y;
	float z = position_world.z;

	int xi = static_cast<int>(x + 0.5);
	int yi = static_cast<int>(y + 0.5);
	int zi = static_cast<int>(z + 0.5);

    RGBLight lightValue = lightData[index3D(xi, yi, zi, 20)];

    if (useShadowTrick) {
        xi = static_cast<int>(x + 0.5 + side_normal.x);
        yi = static_cast<int>(y + 0.5 + side_normal.y);
        zi = static_cast<int>(z + 0.5 + side_normal.z);
        RGBLight secondSample = sample_point_trilinear(xi, yi, zi, lightData);

        if (secondSample.r > lightValue.r) {
            int newValue = static_cast<int>(lightValue.r) - shadowPenalty;
            newValue = std::max(lightMinimum, newValue);
            lightValue.r = static_cast<uint8_t>(newValue);
        }
        if (secondSample.g > lightValue.g) {
            int newValue = static_cast<int>(lightValue.g) - shadowPenalty;
            newValue = std::max(lightMinimum, newValue);
            lightValue.g = static_cast<uint8_t>(newValue);
        }
        if (secondSample.b > lightValue.b) {
            int newValue = static_cast<int>(lightValue.b) - shadowPenalty;
            newValue = std::max(lightMinimum, newValue);
            lightValue.b = static_cast<uint8_t>(newValue);
        }
    }

    return lightValue;
}

inline RGBLight sample_point_trilinear(int x, int y, int z, std::array<RGBLight, 20*20*20> lightData) {
	int accumR = 0;
	int accumG = 0;
	int accumB = 0;
	int total_weight = 0;

    // checks 8 neighboring voxels
	for (int dz = 0; dz <= 1; ++dz) {
		for (int dy = 0; dy <= 1; ++dy) {
			for (int dx = 0; dx <= 1; ++dx) {
				int nx = x + dx;
				int ny = y + dy;
				int nz = z + dz;

				RGBLight l = lightData[index3D(nx, ny, nz, 20)];

                if (l.r == 0 && l.g == 0 && l.b == 0)
                    continue;

				accumR += l.r;
				accumG += l.g;
				accumB += l.b;
				total_weight++;
			}
		}
	}

	if (total_weight > 0) {
		uint8_t r = accumR / total_weight;
		uint8_t g = accumG / total_weight;
		uint8_t b = accumB / total_weight;
        return RGBLight{r, g, b};
	}
	return RGBLight{0, 0, 0};
}

inline RGBLight sample_lighting(zylann::Vector3f position_world, zylann::Vector3f vertex_position, zylann::Vector3f side_normal, std::array<RGBLight, 20*20*20> lightData, int lightMinimum, bool useShadowTrick, int shadowPenalty) {
    position_world += vertex_position;
    position_world += zylann::Vector3f(1.0, 1.0, 1.0);
	float x = position_world.x;
	float y = position_world.y;
	float z = position_world.z;

	int xi = static_cast<int>(x + 0.5);
	int yi = static_cast<int>(y + 0.5);
	int zi = static_cast<int>(z + 0.5);

    RGBLight lightValue = sample_point_trilinear(xi, yi, zi, lightData);

    if (useShadowTrick) {
        xi = static_cast<int>(x + 0.5 + side_normal.x);
        yi = static_cast<int>(y + 0.5 + side_normal.y);
        zi = static_cast<int>(z + 0.5 + side_normal.z);
        RGBLight secondSample = sample_point_trilinear(xi, yi, zi, lightData);

        if (secondSample.r < lightValue.r) {
            int newValue = static_cast<int>(lightValue.r) - shadowPenalty;
            newValue = std::max(lightMinimum, newValue);
            lightValue.r = static_cast<uint8_t>(newValue);
        }
        if (secondSample.g < lightValue.g) {
            int newValue = static_cast<int>(lightValue.g) - shadowPenalty;
            newValue = std::max(lightMinimum, newValue);
            lightValue.g = static_cast<uint8_t>(newValue);
        }
        if (secondSample.b < lightValue.b) {
            int newValue = static_cast<int>(lightValue.b) - shadowPenalty;
            newValue = std::max(lightMinimum, newValue);
            lightValue.b = static_cast<uint8_t>(newValue);
        }
    }

    return lightValue;
}

#endif
