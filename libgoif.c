#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>

int num_frames;
int current_frame;
int width, height;
void** frames;
unsigned long* sizes;

void set_frames(int num, int new_width, int new_height) {
	num_frames = num;
	width = new_width;
	height = new_height;

	if (frames) free(frames);
	frames = (void**)malloc(sizeof(void*) * num);

	if (sizes) free(sizes);
	sizes = (unsigned long*)malloc(sizeof(unsigned long) * num);

	current_frame = 0;
}

void push_frame(void* data, unsigned long size) {
	sizes[current_frame] = size;
	frames[current_frame] = data;
	current_frame++;
}

int trans_index = 0;

typedef struct palette_t {
	int depth;

	unsigned char red[256];
	unsigned char green[256];
	unsigned char blue[256];

	unsigned char split_elt[255];
	unsigned char split[255];
} palette;

int _max(int l, int r) { return l > r ?  l : r; }
int _min(int l, int r) { return l < r ?  l : r; }
int _abs(int i)        { return i < 0 ? -i : i; }


void closest_palette(palette* pal, int red, int green, int blue, int* best_ind, int* best_diff, int root) {
	if (root > (1 << pal->depth) - 1) {
		int ind = root - (1 << pal->depth);
		if (ind == trans_index) return;

		int r_err = red - ((int)pal->red[ind]);
		int g_err = green - ((int)pal->green[ind]);
		int b_err = blue - ((int)pal->blue[ind]);
		int diff = _abs(r_err) + _abs(g_err) + _abs(b_err);

		if (diff < *best_diff) {
			*best_ind = ind;
			*best_diff = diff;
		}

		return;
	}

	int comps[3]; 
	comps[0] = red; 
	comps[1] = green; 
	comps[2] = blue;
	
	int comp = comps[pal->split_elt[root]];

	int pos = pal->split[root];
	if (pos > comp) {
		closest_palette(pal, red, green, blue, best_ind, best_diff, root * 2);
		if (*best_diff > pos - comp) {
			closest_palette(pal, red, green, blue, best_ind, best_diff, root * 2 + 1);
		}
	} else {
		closest_palette(pal, red, green, blue, best_ind, best_diff, root * 2 + 1);
		if (*best_diff > comp - pos) {
			closest_palette(pal, red, green, blue, best_ind, best_diff, root * 2);
		}
	}
}

void swap(unsigned char* image, int a, int b) {
	unsigned char r_a = image[a * 4];
	unsigned char g_a = image[a * 4 + 1];
	unsigned char b_a = image[a * 4 + 2];
	unsigned char a_a = image[a * 4 + 3];

	unsigned char r_b = image[b * 4];
	unsigned char g_b = image[b * 4 + 1];
	unsigned char b_b = image[b * 4 + 2];
	unsigned char a_b = image[a * 4 + 3];

	image[a * 4] = r_b;
	image[a * 4 + 1] = g_b;
	image[a * 4 + 2] = b_b;
	image[a * 4 + 3] = a_b;

	image[b * 4] = r_a;
	image[b * 4 + 1] = g_a;
	image[b * 4 + 2] = b_a;
	image[b * 4 + 3] = a_a;
}

int partition(unsigned char* image, int left, int right, int elt, int pivot_index) {
	int value = image[pivot_index * 4 + elt];
	swap(image, pivot_index, right - 1);
	int index = left;
	int split = 0;
	for (int i = left; i < right - 1; i++)
	{
		int val = image[i * 4 + elt];
		if (val < value)
		{
			swap(image, i, index);
			index++;
		} else if (val == value) {
			if (split) {
				swap(image, i, index);
				index++;
			}

			split = split == 0 ? 1 : 0;
		}
	}
	swap(image, index, right - 1);
	return index;
}

void median(unsigned char* image, int left, int right, int com, int center) {
	if (left < right - 1) {
		int index = left + (right - left) / 2;

		index = partition(image, left, right, com, index);

		if (index > center) {
			median(image, left, index, com, center);
		}

		if (index < center) {
			median(image, index + 1, right, com, center);
		}
	}
}

void split(unsigned char* image, int num_pixels, int first, int last, int split_pos, int dist, int node, palette* pal) {
	if (last <= first || num_pixels == 0) {
		return;
	}

	if (last == first + 1) {
		unsigned long long r = 0, g = 0, b = 0;
		for (int i = 0; i < num_pixels; i++) {
			r += image[i * 4 + 0];
			g += image[i * 4 + 1];
			b += image[i * 4 + 2];
		}

		r += num_pixels / 2; 
		g += num_pixels / 2;
		b += num_pixels / 2;

		r /= num_pixels;
		g /= num_pixels;
		b /= num_pixels;

		pal->red[first] = (unsigned char)r;
		pal->green[first] = (unsigned char)g;
		pal->blue[first] = (unsigned char)b;

		return;
	}

	int min_r = 255, max_r = 0;
	int min_g = 255, max_g = 0;
	int min_b = 255, max_b = 0;
	
	for (int i = 0; i < num_pixels; i++) {
		int r = image[i * 4 + 0];
		int g = image[i * 4 + 1];
		int b = image[i * 4 + 2];

		if (r > max_r) max_r = r;
		if (r < min_r) min_r = r;

		if (g > max_g) max_g = g;
		if (g < min_g) min_g = g;

		if (b > max_b) max_b = b;
		if (b < min_b) min_b = b;
	}

	int r_range = max_r - min_r;
	int g_range = max_g - min_g;
	int b_range = max_b - min_b;

	int com = 1;
	if (b_range > g_range) com = 2;
	if (r_range > b_range && r_range > g_range) com = 0;

	int sub_a = num_pixels * (split_pos - first) / (last - first);
	int sub_b = num_pixels - sub_a;

	median(image, 0, num_pixels, com, sub_a);

	pal->split_elt[node] = com;
	pal->split[node] = image[sub_a * 4 + com];

	split(image, sub_a, first, split_pos, split_pos - dist, dist / 2, node * 2, pal);
	split(image + sub_a * 4, sub_b, split_pos, last, split_pos + dist, dist / 2, node * 2 + 1, pal);
}

int pick_changed(unsigned char* last, unsigned char* frame, int num_pixels) {
	int num_changed = 0;
	unsigned char* write_iter = frame;

	for (int i = 0; i < num_pixels; i++) {
		if (last[0] != frame[0] || last[1] != frame[1] ||
				last[2] != frame[2]) {
			write_iter[0] = frame[0];
			write_iter[1] = frame[1];
			write_iter[2] = frame[2];
			num_changed++;
			write_iter += 4;
		}

		last += 4;
		frame += 4;
	}

	return num_changed;
}

void make_palette(unsigned char* last, unsigned char* next, unsigned int width, unsigned int height, int depth, palette* pal) {
	pal->depth = depth;

	int size = width * height * 4 * sizeof(unsigned char);
	unsigned char* image = (unsigned char*)malloc(size);
	memcpy(image, next, size);

	int num_pixels = width * height;
	if (last) {
		num_pixels = pick_changed(last, image, num_pixels);
	}

	int last_elt = 1 << depth;
	int split_elt = last_elt / 2;
	int dist = split_elt / 2;

	split(image, num_pixels, 1, last_elt, split_elt, dist, 1, pal);

	free(image);

	pal->split[1 << (depth - 1)] = 0;
	pal->split_elt[1 << (depth - 1)] = 0;

	pal->red[0] = pal->green[0] = pal->blue[0] = 0;
}

void threshold(unsigned char* last, unsigned char* next, unsigned char* out, unsigned int width, unsigned int height, palette* pal) {
	unsigned int num_pixels = width * height;
	for (unsigned int i = 0; i < num_pixels; i++) {
		if (last && last[0] == next[0] &&
				last[1] == next[1] && last[2] == next[2]) {
			out[0] = last[0];
			out[1] = last[1];
			out[2] = last[2];
			out[3] = trans_index;
		} else {
			int best_diff = 1000000;
			int best_ind = 1;
			closest_palette(pal, next[0], next[1], next[2], &best_ind, &best_diff, 1);

			out[0] = pal->red[best_ind];
			out[1] = pal->green[best_ind];
			out[2] = pal->blue[best_ind];
			out[3] = best_ind;
		}

		if (last) last += 4;
		out += 4;
		next += 4;
	}
}

typedef struct bit_buffer_t {
	unsigned char bit;
	unsigned char byte;

	unsigned int index;
	unsigned char buffer[256];
} bit_buffer;

void write_bit(bit_buffer* buffer, unsigned int bit) {
	bit = bit & 1;
	bit = bit << buffer->bit;
	buffer->byte |= bit;

	buffer->bit++;
	if (buffer->bit > 7) {
		buffer->buffer[buffer->index++] = buffer->byte;
		buffer->bit = 0;
		buffer->byte = 0;
	}
}

void write_chunk(FILE* f, bit_buffer* buffer) {
	fputc(buffer->index, f);
	fwrite(buffer->buffer, 1, buffer->index, f);

	buffer->bit = 0;
	buffer->byte = 0;
	buffer->index = 0;
}

void write_code(FILE* f, bit_buffer* buffer, unsigned int code, unsigned int length) {
	for (unsigned int i = 0; i < length; i++) {
		write_bit(buffer, code);
		code = code >> 1;

		if (buffer->index == 255) {
			write_chunk(f, buffer);
		}
	}
}

typedef struct lzw_package_t {
	unsigned short blocks[256];
} lzw_package;

void write_palette(palette* pal, FILE* f) {
	fputc(0, f); 
	fputc(0, f);
	fputc(0, f);

	for (int i = 1; i < (1 << pal->depth); i++) {
		unsigned int r = pal->red[i];
		unsigned int g = pal->green[i];
		unsigned int b = pal->blue[i];

		fputc(r, f);
		fputc(g, f);
		fputc(b, f);
	}
}

void write_lzw(FILE* f, unsigned char* image, unsigned int left, unsigned int top, unsigned int width, unsigned int height, unsigned int delay, palette* pal) {
	fputc(0x21, f);
	fputc(0xf9, f);
	fputc(0x04, f);
	fputc(0x05, f);
	fputc(delay & 0xff, f);
	fputc((delay >> 8) & 0xff, f);
	fputc(trans_index, f);
	fputc(0, f);
	fputc(0x2c, f);
	fputc(left & 0xff, f);
	fputc((left >> 8) & 0xff, f);
	fputc(top & 0xff, f);
	fputc((top >> 8) & 0xff, f);
	fputc(width & 0xff, f);
	fputc((width >> 8) & 0xff, f);
	fputc(height & 0xff, f);
	fputc((height >> 8) & 0xff, f);
	fputc(0x80 + pal->depth - 1, f);
	write_palette(pal, f);

	int min_size = pal->depth;
	unsigned int clear = 1 << pal->depth;

	fputc(min_size, f);

	lzw_package* package = (lzw_package*)malloc(sizeof(lzw_package) * 4096);

	memset(package, 0, sizeof(lzw_package) * 4096);
	int current = -1;
	unsigned int size = min_size + 1;
	unsigned int code = clear + 1;

	bit_buffer buffer;
	buffer.byte = 0;
	buffer.bit = 0;
	buffer.index = 0;

	write_code(f, &buffer, clear, size);

	for (unsigned int yy = 0; yy < height; yy++) {
		for (unsigned int xx = 0; xx < width; xx++) {
			unsigned char next_value = image[(yy * width + xx) * 4 + 3];

			if (current < 0) {
				current = next_value;
			}
			else if (package[current].blocks[next_value]) {
				current = package[current].blocks[next_value];
			} else {
				write_code(f, &buffer, current, size);
				package[current].blocks[next_value] = ++code;

				if (code >= (1ul << size)) {
					size++;
				}

				if (code == 4095) {
					write_code(f, &buffer, clear, size);

					memset(package, 0, sizeof(lzw_package) * 4096);
					current = -1;
					size = min_size + 1;
					code = clear + 1;
				}

				current = next_value;
			}
		}
	}

	write_code(f, &buffer, current, size);
	write_code(f, &buffer, clear, size);
	write_code(f, &buffer, clear + 1, min_size + 1);

	while (buffer.bit) write_bit(&buffer, 0);
	if (buffer.index) write_chunk(f, &buffer);

	fputc(0, f);

	free(package);
}

typedef struct writer_t {
	FILE* f;
	unsigned char* old;
	int first;
} writer;

int begin(writer* writer, char* filename, unsigned int width, unsigned int height, unsigned int delay, int depth) {
	writer->f = 0;
#ifdef _WIN32
    fopen_s(&writer->f, filename, "wb");
#else
    writer->f = fopen(filename, "wb");
#endif
	if (depth == 0) depth = 8;

	if (!writer->f) return 0;

	writer->first = 1;

	writer->old = (unsigned char*)malloc(width * height * 4);

	fputs("GIF89a", writer->f);

	fputc(width & 0xff, writer->f);
	fputc((width >> 8) & 0xff, writer->f);
	fputc(height & 0xff, writer->f);
	fputc((height >> 8) & 0xff, writer->f);

	fputc(0xf0, writer->f);  
	fputc(0, writer->f);     
	fputc(0, writer->f);     
	fputc(0, writer->f);
	fputc(0, writer->f);
	fputc(0, writer->f);
	fputc(0, writer->f);
	fputc(0, writer->f);
	fputc(0, writer->f);

	fputc(0x21, writer->f);
	fputc(0xff, writer->f);
	fputc(11, writer->f);
	fputs("NETSCAPE2.0", writer->f);
	fputc(3, writer->f);
	fputc(1, writer->f);
	fputc(0, writer->f);
	fputc(0, writer->f);
	fputc(0, writer->f);

	return 1;
}

int frame(writer* writer, unsigned char* image, unsigned int width, unsigned int height, unsigned int delay, int depth){
	if (!writer->f) return 0;
	if (depth == 0) depth = 8;

	unsigned char* oldImage = writer->first ? NULL : writer->old;
	writer->first = 0;

	palette pal;
	make_palette(oldImage, image, width, height, depth, &pal);

	threshold(oldImage, image, writer->old, width, height, &pal);

	write_lzw(writer->f, writer->old, 0, 0, width, height, delay, &pal);

	return 1;
}

int end(writer* writer) {
	if (!writer->f) return 0;

	fputc(0x3b, writer->f);
	fclose(writer->f);
	free(writer->old);

	writer->f = NULL;
	writer->old = NULL;

	return 1;
}

void save(char* path, int verbose) {
	if(verbose) printf("GIF_START\n");

	writer writer;
	begin(&writer, path, width, height, 2, 8);
	
	for (int i = 0; i < num_frames; i++) {
		if (verbose) printf("GIF_%d\n", i);
		frame(&writer, (unsigned char*)frames[i], width, height, 2, 8);
	}
	
	end(&writer);

	free(frames);
	free(sizes);

	if (verbose) printf("GIF_END\n");
}

int l_set_frames(lua_State* L) {
	int num = lua_tointeger(L, 1);
	int w = lua_tointeger(L, 2);
	int h = lua_tointeger(L, 3);
    set_frames(num, w, h);
	return 0;
}

int l_push_frame(lua_State* L) {
	void* data = lua_touserdata(L, 1);
	unsigned long size = (unsigned long)lua_tonumber(L, 2);
	int verbose = lua_toboolean(L, 3);
	if (verbose) printf("PUSH_%d\n", current_frame);
	push_frame(data, size);
	return 0;
}

int l_save(lua_State* L) {
	const char* path = lua_tostring(L, 1);
	int verbose = lua_toboolean(L, 2);
	if (verbose) printf("SAVE_START\n");
	save((char*)path, verbose);
	if (verbose) printf("SAVE_END\n");
	return 0;
}

static luaL_reg lib[] = {
	{"set_frames", l_set_frames},
	{"push_frame", l_push_frame},
	{"save", l_save},
	{NULL, NULL}
};

int 
#ifdef _WIN32
__declspec(dllexport)
#endif
luaopen_libgoif(lua_State* L) {
	luaL_openlib(L, "goif", lib, 0);
	return 1;
}
