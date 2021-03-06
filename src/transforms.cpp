#include "transforms.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <array>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

using namespace std;

locker::locker(image_io& image_src) : m_image(image_src) {
	// Lock the image
	if (SDL_MUSTLOCK(m_image.get_image())) {
		SDL_LockSurface(m_image.get_image());
	}
}

locker::~locker() {
	// Unlock the image
	if (SDL_MUSTLOCK(m_image.get_image())) {
		SDL_UnlockSurface(m_image.get_image());
	}
}

void color_mask(image_io& image_src, int c_mask) {
	locker lock(image_src);

	// Holds pixel data for reading and writing
	Uint32 pixel_src, pixel_dst;

	Uint32 red_value, green_value, blue_value;

	// Iterate through every pixel
	for (int x = 0; x < image_src.get_image()->w; x++) {
		red_value = green_value = blue_value = 0;

		for (int y = 0; y < image_src.get_image()->h; y++) {
			pixel_src = image_src.get_pixel(x, y);

			// Strip colors depending on c_mask
			if (((c_mask & M_RED) != M_RED)) red_value = RGB_to_red(pixel_src);
			if (((c_mask & M_GREEN) != M_GREEN)) green_value = RGB_to_green(pixel_src);
			if (((c_mask & M_BLUE) != M_BLUE)) blue_value = RGB_to_blue(pixel_src);

			// If all colors are masked, display in grayscale
			if ((c_mask == (M_RED | M_GREEN | M_BLUE))) {
				red_value = green_value = blue_value = RGB_to_gray(pixel_src);
			}

			pixel_dst = pack_RGB(red_value, green_value, blue_value);

			image_src.put_pixel(x, y, pixel_dst);
		}
	}
}

void invert(image_io& image_src) {
	locker lock(image_src);

	// Holds pixel data for reading and writing
	Uint32 pixel_src, pixel_dst;

	// Iterate through every pixel
	for (int x = 0; x < image_src.get_image()->w; x++) {
		for (int y = 0; y < image_src.get_image()->h; y++) {
			pixel_src = image_src.get_pixel(x, y);

			// Invert the color
			pixel_dst = ((255 - ((pixel_src >> 0) & 0xFF)) << 0)
						| ((255 - ((pixel_src >> 8) & 0xFF)) << 8)
						| ((255 - ((pixel_src >> 16) & 0xFF)) << 16);

			image_src.put_pixel(x, y, pixel_dst);
		}
	}
}

void smooth_mean(image_io& image_src) {
	// Create a copy for use in algorithms
	image_io image_tmp(image_src);

	locker lock(image_src);
	locker lock_tmp(image_tmp);

	// Holds pixel data for reading and writing
	Uint32 pixel_src, pixel_dst;

	// Variable to hold the pixel average throughout the neighborhood
	int R_avg;
	int G_avg;
	int B_avg;

	// Iterate through every pixel, skip the outer edges
	for (int x = 1; x < image_tmp.get_image()->w - 1; x++) {
		for (int y = 1; y < image_tmp.get_image()->h - 1; y++) {
			// Variable to hold the pixel average throughout the neighborhood
			R_avg = G_avg = B_avg = 0;

			// Iterate through the neighborhood
			for (int u = -1; u + 1 < 3; u++) {
				for (int v = -1; v + 1 < 3; v++) {
					pixel_src = image_tmp.get_pixel(x + u, y + v);

					// Iterate through the 9 pixels in the neighborhood
					// Each has an equal weight of 1/9
					R_avg += ((pixel_src >> 0) & 0xFF);
					G_avg += ((pixel_src >> 8) & 0xFF);
					B_avg += ((pixel_src >> 16) & 0xFF);
				}
			}

			// Pack the color averages back into a single pixel
			pixel_dst = (R_avg/9 << 0)
						| (G_avg/9 << 8)
						| (B_avg/9 << 16);

			image_src.put_pixel(x, y, pixel_dst);
		}
	}
}

void smooth_median(image_io& image_src) {
	// Create a copy for use in algorithms
	image_io image_tmp(image_src);

	locker lock(image_src);
	locker lock_tmp(image_tmp);

	// Holds pixel data for reading and writing
	Uint32 pixel_src, pixel_dst;

	int R_med;
	int G_med;
	int B_med;

	int R_list[9];
	int G_list[9];
	int B_list[9];

	// Iterate through every pixel, skip the outer edges
	for (int x = 1; x < image_tmp.get_image()->w - 1; x++) {
		for (int y = 1; y < image_tmp.get_image()->h - 1; y++) {
			// Iterate through the neighborhood
			for (int u = -1; u + 1 < 3; u++) {
				for (int v = -1; v + 1 < 3; v++) {
					pixel_src = image_tmp.get_pixel(x + u, y + v);

					// Iterate through the 9 pixels in the neighborhood
					R_list[(u + 1) + 3*(v + 1)] = ((pixel_src >> 0) & 0xFF);
					G_list[(u + 1) + 3*(v + 1)] = ((pixel_src >> 8) & 0xFF);
					B_list[(u + 1) + 3*(v + 1)] = ((pixel_src >> 16) & 0xFF);
				}
			}

			// Sort the lists
			std::sort(R_list, R_list + 9);
			std::sort(G_list, G_list + 9);
			std::sort(B_list, B_list + 9);

			// Choose the middle value
			R_med = R_list[4];
			G_med = G_list[4];
			B_med = B_list[4];

			// Pack the color medians back into a single pixel
			pixel_dst = (R_med << 0)
						| (G_med << 8)
						| (B_med << 16);

			image_src.put_pixel(x, y, pixel_dst);
		}
	}
}

void hist_eq(image_io& image_src) {
	locker lock(image_src);

	// Holds pixel data for reading and writing
	Uint32 pixel_src, pixel_dst;

	Uint32 red_level_sum[256];
	Uint32 green_level_sum[256];
	Uint32 blue_level_sum[256];

	long int red_level_integral[256];
	long int green_level_integral[256];
	long int blue_level_integral[256];

	Uint8 red_value, green_value, blue_value;
	Uint32 red_value_unscaled, green_value_unscaled, blue_value_unscaled;
	Uint32 red_value_scaled, green_value_scaled, blue_value_scaled;

	for (int i = 0; i <= 255; i++) {
		red_level_sum[i] = 0;
		green_level_sum[i] = 0;
		blue_level_sum[i] = 0;

		red_level_integral[i] = 0;
		green_level_integral[i] = 0;
		blue_level_integral[i] = 0;
	}

	// Iterate through every pixel and measure the intensity
	for (int x = 0; x < image_src.get_image()->w; x++) {
		for (int y = 0; y < image_src.get_image()->h; y++) {
			pixel_src = image_src.get_pixel(x, y);

			red_value = RGB_to_red(pixel_src);
			green_value = RGB_to_green(pixel_src);
			blue_value = RGB_to_blue(pixel_src);

			// Increment the count of that intensity
			// This is data for the histogram
			red_level_sum[red_value] += 1;
			green_level_sum[green_value] += 1;
			blue_level_sum[blue_value] += 1;
		}
	}

	// Compute the first term of the integral
	red_level_integral[0] = red_level_sum[0];
	green_level_integral[0] = green_level_sum[0];
	blue_level_integral[0] = blue_level_sum[0];

	// Compute the integral
	for (int j = 1; j <= 255; j++) {
		// Integrate over the gray value intensity levels
		red_level_integral[j] = (red_level_sum[j] + red_level_integral[j - 1]);
		green_level_integral[j] = (green_level_sum[j] + green_level_integral[j - 1]);
		blue_level_integral[j] = (blue_level_sum[j] + blue_level_integral[j - 1]);
	}

	// Iterate through every pixel and adjust the intensity
	for (int x = 0; x < image_src.get_image()->w; x++) {
		for (int y = 0; y < image_src.get_image()->h; y++) {
			pixel_src = image_src.get_pixel(x, y);

			// Separate into the red/green/blue intensities
			red_value = RGB_to_red(pixel_src);
			green_value = RGB_to_green(pixel_src);
			blue_value = RGB_to_blue(pixel_src);

			// Use the integral as the transfer function of each pixel
			red_value_unscaled = red_level_integral[red_value];
			green_value_unscaled = green_level_integral[green_value];
			blue_value_unscaled = blue_level_integral[blue_value];

			red_value_scaled = 255.0*red_value_unscaled/((double) red_level_integral[255]);
			green_value_scaled = 255.0*green_value_unscaled/((double) green_level_integral[255]);
			blue_value_scaled = 255.0*blue_value_unscaled/((double) blue_level_integral[255]);

			// Pack the color averages back into a single pixel
			pixel_dst = pack_RGB(red_value_scaled,
										green_value_scaled,
										blue_value_scaled);

			// Write to the image
			image_src.put_pixel(x, y, pixel_dst);
		}
	}
}

// Convert an image into a binary (black/white) image splitting at the threshold. All pixels equal to or greater than the threshold will be turned white, all pixels below will be black
void threshold(image_io& image_src, Uint32 threshold) {
	locker lock(image_src);

	// Holds pixel data for reading and writing
	Uint32 pixel_src, pixel_dst;
	Uint32 gray_value, bw_value;

	// Iterate through every pixel
	for (int x = 0; x < image_src.get_image()->w; x++) {
		gray_value = 0;

		for (int y = 0; y < image_src.get_image()->h; y++) {
			pixel_src = image_src.get_pixel(x, y);

			// Get the gray value of each pixel
			gray_value = RGB_to_gray(pixel_src);

			bw_value = (gray_value >= threshold)?0xFF:0x00;

			pixel_dst = pack_RGB(bw_value, bw_value, bw_value);

			image_src.put_pixel(x, y, pixel_dst);
		}
	}
}

// Edge detection using the Sobel Gradient
void sobel_gradient(image_io& image_src) {
	// Create a copy for use in algorithms
	image_io image_tmp(image_src);

	locker lock(image_src);
	locker lock_tmp(image_tmp);

	// Holds pixel data for reading and writing
	Uint32 pixel_src, pixel_dst;

	// Get the gray value of each pixel
	Uint32 gray_value;

	int gray_value_sum_x, gray_value_sum_y, gray_value_sum_xy;

	// Iterate through every pixel, skip the outer edges
	for (int x = 1; x < image_tmp.get_image()->w - 1; x++) {
		for (int y = 1; y < image_tmp.get_image()->h - 1; y++) {
			// Variable to hold the pixel average throughout the neighborhood
			gray_value_sum_x = gray_value_sum_y = gray_value_sum_xy = 0;

			// Sobel mask in the x-direction
			static int sobel_mask_x[] = {-1, 0, 1,
										-2, 0, 2,
										-1, 0, 1};

			// Sobel mask in the y-direction
			static int sobel_mask_y[] = {-1, -2, -1,
										0, 0, 0,
										1, 2, 1};

			// Iterate through the neighborhood
			for (int u = -1; u + 1 < 3; u++) {
				for (int v = -1; v + 1 < 3; v++) {
					pixel_src = image_tmp.get_pixel(x + u, y + v);

					// Get the gray value of each pixel
					gray_value = RGB_to_gray(pixel_src);

					// Iterate through the 9 pixels in the neighborhood
					// Each has a weight determined by the Sobel mask
					gray_value_sum_x += sobel_mask_x[(u + 1) + 3*(v + 1)]*gray_value;
					gray_value_sum_y += sobel_mask_y[(u + 1) + 3*(v + 1)]*gray_value;
				}
			}

			// Combine the x and y gradients with Pythagorean theorem
			gray_value_sum_xy = sqrt(pow(gray_value_sum_x, 2) + pow(gray_value_sum_y, 2));

			// Pack the color averages back into a single pixel
			pixel_dst = pack_RGB(gray_value_sum_xy, gray_value_sum_xy, gray_value_sum_xy);

			image_src.put_pixel(x, y, pixel_dst);
		}
	}
}

// Edge detection using the Sobel Gradient
void laplacian(image_io& image_src) {
	// Create a copy for use in algorithms
	image_io image_tmp(image_src);

	locker lock(image_src);
	locker lock_tmp(image_tmp);

	// Holds pixel data for reading and writing
	Uint32 pixel_src, pixel_dst;

	// Get the gray value of each pixel
	Uint32 gray_value;
	int gray_value_sum;

	// Iterate through every pixel, skip the outer edges
	for (int x = 1; x < image_tmp.get_image()->w - 1; x++) {
		for (int y = 1; y < image_tmp.get_image()->h - 1; y++) {
			// Variable to hold the pixel average throughout the neighborhood
			gray_value_sum = 0;

			// Laplace mask
			static int laplacian_mask[] = {0, 1, 0,
											1, -4, 1,
											0, 1, 0};

			// Iterate through the neighborhood
			for (int u = -1; u + 1 < 3; u++) {
				for (int v = -1; v + 1 < 3; v++) {
					pixel_src = image_tmp.get_pixel(x + u, y + v);

					// Get the gray value of each pixel
					gray_value = RGB_to_gray(pixel_src);

					// Iterate through the 9 pixels in the neighborhood
					gray_value_sum += laplacian_mask[(u + 1) + 3*(v + 1)]*gray_value;
				}
			}

			// Pack the color averages back into a single pixel
			pixel_dst = pack_RGB(gray_value_sum, gray_value_sum, gray_value_sum);

			image_src.put_pixel(x, y, pixel_dst);
		}
	}
}

// Degrade the image by n pixels
// Erodes away black objects
// Doesn't like pngs created by MS Paint
void erosion(image_io& image_src, int erode_n) {
	locker lock(image_src);

	// Holds pixel data for reading and writing
	Uint32 pixel_src, pixel_dst;
	Uint8 gray_value;

	int erode_flag;

	for (int n = 0; n < erode_n; n++) {
		// Create a copy for use in algorithms
		image_io image_tmp(image_src);
		locker lock_tmp(image_tmp);

		// Iterate through every pixel, skip the outer edges
		for (int x = 1; x < image_tmp.get_image()->w - 1; x++) {
			for (int y = 1; y < image_tmp.get_image()->h - 1; y++) {
				erode_flag = 0;

				// Iterate through the neighborhood
				for (int u = -1; u + 1 < 3; u++) {
					for (int v = -1; v + 1 < 3; v++) {
						pixel_src = image_tmp.get_pixel(x + u, y + v);

						// Get the gray value of each pixel
						gray_value = RGB_to_gray(pixel_src);

						// If pixels in the neighborhood aren't black erode
						if (gray_value != 0x00) {
							erode_flag = 1;

							break;
						}
					}
				}

				if (erode_flag) {
					// Change this pixel to white
					pixel_dst = pack_RGB(0xFF, 0xFF, 0xFF);

					image_src.put_pixel(x, y, pixel_dst);
				}
			}
		}
	}
}

// Enlarge the image by n pixels
// Dilates black
void dilation(image_io& image_src, int dilate_n) {
	locker lock(image_src);

	// Holds pixel data for reading and writing
	Uint32 pixel_src, pixel_dst;

	Uint8 gray_value;

	for (int n = 0; n < dilate_n; n++) {
		// Create a copy for use in algorithms
		image_io image_tmp(image_src);
		locker lock_tmp(image_tmp);

		// Iterate through every pixel, skip the outer edges
		for (int x = 1; x < image_tmp.get_image()->w - 1; x++) {
			for (int y = 1; y < image_tmp.get_image()->h - 1; y++) {
				pixel_src = image_tmp.get_pixel(x, y);

				// Get the gray value of each pixel
				gray_value = RGB_to_gray(pixel_src);

				// Pack the color averages back into a single pixel
				pixel_dst = pack_RGB(0x00, 0x00, 0x00);

				if (gray_value == 0x00) {
					// Iterate through the neighborhood
					for (int u = -1; u + 1 < 3; u++) {
						for (int v = -1; v + 1 < 3; v++) {
							// Fill in a 3x3 mask of pixels
							image_src.put_pixel(x + u, y + v, pixel_dst);
						}
					}
				}
			}
		}
	}
}

// Compute the perimeter
int perimiter(image_io& image_src) {
	// Create a copy for use in algorithms
	image_io image_eroded(image_src);

	locker lock(image_src);

	// Erode the image, take difference between eroded and normal and sum
	erosion(image_eroded, 1);

	// Holds pixel data for reading and writing
	Uint32 pixel_src, pixel_src_eroded;
	Uint32 pixel_src_gray, pixel_src_gray_eroded;
	int perimeter_sum = 0;

	// Iterate through every pixel, skip the outer edges
	for (int x = 1; x < image_eroded.get_image()->w - 1; x++) {
		for (int y = 1; y < image_eroded.get_image()->h - 1; y++) {
			pixel_src = image_src.get_pixel(x, y);
			pixel_src_eroded = image_eroded.get_pixel(x, y);

			pixel_src_gray = RGB_to_gray(pixel_src);
			pixel_src_gray_eroded = RGB_to_gray(pixel_src_eroded);

			// If pixels are different they must be part of the perimiter
			if (abs(pixel_src_gray - pixel_src_gray_eroded)) {
				perimeter_sum++;
			}
		}
	}

	return perimeter_sum;
}

// Compute the area
int area(image_io& image_src) {
	locker lock(image_src);

	// Holds pixel data for reading and writing
	Uint32 pixel_src;
	Uint32 pixel_src_gray;
	int area_sum = 0;

	// Iterate through every pixel
	for (int x = 1; x < image_src.get_image()->w - 1; x++) {
		for (int y = 1; y < image_src.get_image()->h - 1; y++) {
			pixel_src = image_src.get_pixel(x, y);

			pixel_src_gray = RGB_to_gray(pixel_src);

			// If a pixel is black, add it to the sum
			if (!pixel_src_gray) {
				area_sum++;
			}
		}
	}

	return area_sum;
}

// Compute the moments
// Mij = ExEy x^i*y^j*I(x, y)
std::array<std::array<double, 4>, 4> moment(image_io& image_src) {
	locker lock(image_src);

	// Holds pixel data for reading and writing
	Uint32 pixel_src, gray_value;

	// Need to compute M00, M01, M02, M03
	// M10, M20, M30
	// M11, M12, M21
	std::array<std::array<double, 4>, 4> M = {0};

	// Iterate through every pixel
	for (int x = 0; x < image_src.get_image()->w; x++) {
		for (int y = 0; y < image_src.get_image()->h; y++) {
			pixel_src = image_src.get_pixel(x, y);

			// Get the gray value of each pixel
			// Subtract from 255 to moments of the black pixels
			gray_value = 255 - RGB_to_gray(pixel_src);

			M[0][0] += gray_value;
			M[0][1] += gray_value*y;
			M[0][2] += gray_value*y*y;
			M[0][3] += gray_value*y*y*y;
			M[1][0] += gray_value*x;
			M[2][0] += gray_value*x*x;
			M[3][0] += gray_value*x*x*x;
			M[1][1] += gray_value*x*y;
			M[1][2] += gray_value*x*y*y;
			M[2][1] += gray_value*x*x*y;
		}
	}

	return M;
}

// Compute the centroid from the moment
// Returns an array of two (x, y)
// M is a 4x4 matrix containing values of the moments
std::array<double, 2> centroid(const std::array<std::array<double, 4>, 4>& M) {
	// Allocate memory for the centroid value
	std::array<double, 2> C = {0};

	// Compute the x and y components of the centroid
	C[0] = M[1][0]/M[0][0];
	C[1] = M[0][1]/M[0][0];

	return C;
}

// Compute the central_moments
// Returns an array of the central moments
std::array<std::array<double, 4>, 4> central_moments(const std::array<std::array<double, 4>, 4>& M, const std::array<double, 2>& C) {
	// u represents the greek letter mu
	std::array<std::array<double, 4>, 4> u = {0};

	u[0][0] = M[0][0];
	u[0][2] = M[0][2] - C[1]*M[0][1];
	u[0][3] = M[0][3] - 3*C[1]*M[0][2] + 2*C[1]*C[1]*M[0][1];
	u[2][0] = M[2][0] - C[0]*M[1][0];
	u[3][0] = M[3][0] - 3*C[0]*M[2][0] + 2*C[0]*C[0]*M[1][0];
	u[1][1] = M[1][1] - C[0]*M[0][1];
	u[1][2] = M[1][2] - 2*C[1]*M[1][1] - C[0]*M[0][2] + 2*C[1]*C[1]*M[1][0];
	u[2][1] = M[2][1] - 2*C[0]*M[1][1] - C[1]*M[2][0] + 2*C[0]*C[0]*M[0][1];

	return u;
}

// Calculate the 7 moment invariants
// u is a 4x4 matrix containing central moment values
std::array<double, 7> invariants(const std::array<std::array<double, 4>, 4>& u) {
	// n represents the greek letter eta
	double n[4][4];
	std::array<double, 7> invariants = {0};

	// eta_ij = u_ij/(pow(u_00, 1 + (i + j)/2))
	n[0][2] = u[0][2]/(pow(u[0][0], 2));
	n[0][3] = u[0][3]/(pow(u[0][0], 2.5));
	n[2][0] = u[2][0]/(pow(u[0][0], 2));
	n[3][0] = u[3][0]/(pow(u[0][0], 2.5));
	n[1][1] = u[1][1]/(pow(u[0][0], 2));
	n[1][2] = u[1][2]/(pow(u[0][0], 2.5));
	n[2][1] = u[2][1]/(pow(u[0][0], 2.5));

	// These are the seven Hu invariants
	// Can be found on
	// http://en.wikipedia.org/wiki/Image_moment#Rotation_invariant_moments
	invariants[0] = n[2][0] + n[0][2];
	invariants[1] = (n[2][0] - n[0][2])*(n[2][0] - n[0][2])
				+ 4*n[1][1]*n[1][1];
	invariants[2] = (n[3][0] - 3*n[1][2])*(n[3][0] - 3*n[1][2])
				+ (3*n[2][1] - n[0][3])*(3*n[2][1] - n[0][3]);
	invariants[3] = (n[3][0] + n[1][2])*(n[3][0] + n[1][2])
				+ (n[2][1] + n[0][3])*(n[2][1] + n[0][3]);
	invariants[4] = (n[3][0] - 3*n[1][2])
				* (n[3][0] + n[1][2])
				* ((n[3][0] + n[1][2])*(n[3][0] + n[1][2])
				- 3*((n[2][1] + n[0][3])*(n[2][1] + n[0][3])))

				* (3*n[2][1] - n[0][3])
				* (n[2][1] + n[0][3])
				* (3*(n[3][0] + n[1][2])*(n[3][0] + n[1][2])
				- ((n[2][1] + n[0][3])*(n[2][1] + n[0][3])));
	invariants[5] = (n[2][0] - n[0][2])
				* ((n[3][0] + n[1][2])*(n[3][0] + n[1][2])
				- (n[2][1] + n[0][3])*(n[2][1] + n[0][3]))

				+ 4*n[1][1]
				* (n[3][0] + n[1][2])
				* (n[2][1] + n[0][3]);
	invariants[6] = (3*n[2][1] - n[0][3])
				* (n[3][0] + n[1][2])
				* ((n[3][0] + n[1][2])*(n[3][0] + n[1][2])
				- 3*((n[2][1] + n[0][3])*(n[2][1] + n[0][3])))

				- (n[3][0] - 3*n[1][2])
				* (n[2][1] + n[0][3])
				* (3*(n[3][0] + n[1][2])*(n[3][0] + n[1][2])
				- ((n[2][1] + n[0][3])*(n[2][1] + n[0][3])));

	return invariants;
}

// Calculate the eigenvalues and eigenvectors of the covariance matrix
std::array<std::array<double, 2>, 3> eigen(const std::array<std::array<double, 4>, 4>& M, const std::array<double, 2>& C) {
	double A[2][2];
	double T, D;
	double up20, up02, up11;

	std::array<std::array<double, 2>, 3> eigen = {0};

	// Calculate values for the covariance matrix
	up20 = M[2][0]/M[0][0] - C[0]*C[0];
	up02 = M[0][2]/M[0][0] - C[1]*C[1];
	up11 = M[1][1]/M[0][0] - C[0]*C[1];

	// Populate the covariance matrix
	A[0][0] = up20;
	A[0][1] = up11;
	A[1][0] = up11;
	A[1][1] = up02;

	// Intermediate value
	T = A[0][0] + A[1][1];
	D = A[0][0]*A[1][1] - A[0][1]*A[1][0];

	// Calculate the eigenvalues
	eigen[0][0] = T/2 + sqrt(((T*T)/4 - D));
	eigen[1][0] = T/2 - sqrt(((T*T)/4 - D));

	// If c isn't zero
	if (A[1][0] != 0) {
		// (L1 - d, c)
		eigen[0][1] = eigen[0][0] - A[1][1];
		eigen[0][2] = A[1][0];

		// (L2 - d, c)
		eigen[1][1] = eigen[1][0] - A[1][1];
		eigen[1][2] = A[1][0];

	// If b isn't zero
	}
	else if (A[0][1] != 0) {
		// (b, L1 - a)
		eigen[0][1] = A[0][1];
		eigen[0][2] = eigen[0][0] - A[0][0];

		// (b, L2 - a)
		eigen[1][1] = A[0][1];
		eigen[1][2] = eigen[1][0] - A[0][0];
	}
	else {
		eigen[0][1] = 1;
		eigen[0][2] = 0;

		eigen[1][1] = 0;
		eigen[1][2] = 1;
	}

	return eigen;
}

// Convert from RGB pixel format to gray value
// gray_value = (0.299*r + 0.587*g + 0.114*b);
Uint8 RGB_to_gray(Uint32 RGB_pixel) {
	// This math shouldn't grow greater than 255 so checking for saturation/overflow isn't necessary
	Uint8 gray_value = .3*((RGB_pixel >> 0) & 0xFF)
					+ .587*((RGB_pixel >> 8) & 0xFF)
					+ .114*((RGB_pixel >> 16) & 0xFF);

	return gray_value;
}

// Return the color components
Uint8 RGB_to_red(Uint32 RGB_pixel) { return ((RGB_pixel >> 0) & 0xFF); }
Uint8 RGB_to_green(Uint32 RGB_pixel) { return ((RGB_pixel >> 8) & 0xFF); }
Uint8 RGB_to_blue(Uint32 RGB_pixel) { return ((RGB_pixel >> 16) & 0xFF); }

// Takes red/green/blue values and packs it back into RGB representation
// Cast to a 32-bit int so the bit shifts don't shift off into nothing
// Values are passed in as 8-bit values to avoid problems where large positive numbers would cause incorrect colors to show up
Uint32 pack_RGB(Uint8 red_value, Uint8 green_value, Uint8 blue_value) {
	Uint32 RGB_pixel = (((Uint32) red_value) << 0)
					| (((Uint32) green_value) << 8)
					| (((Uint32) blue_value) << 16);

	return RGB_pixel;
}
