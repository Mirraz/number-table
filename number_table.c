#define _BSD_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <memory.h>
#include <endian.h>
#include <unistd.h> // getopt, ssize_t
#include <limits.h> // SSIZE_MAX

#define UNSIGNED_STR "u"
#define UNSIGNED_CHAR 'u'
#define SIGNED_STR "s"
#define SIGNED_CHAR 's'

#define NUM_SIZES_COUNT 4
const uint_fast8_t num_sizes[NUM_SIZES_COUNT] = {1, 2, 4, 8};
#define NUM_SIZE_MAX 8

typedef uint64_t uint_max_type;
typedef  int64_t sint_max_type;
#define UINT_MAX_TYPE_MAX UINT64_MAX
#define SINT_MAX_TYPE_MIN_ABS ((uint_max_type)INT64_MAX+1)
#define SINT_MAX_TYPE_MIN INT64_MIN
#define SINT_MAX_TYPE_MAX INT64_MAX
const char SCN_SINT_MAX_TYPE[] = "%" SCNd64;
const char SCN_UINT_MAX_TYPE[] = "%" SCNu64;
const char PRI_SINT_MAX_TYPE[] = "%" PRId64;
const char PRI_UINT_MAX_TYPE[] = "%" PRIu64;

typedef union {
	uint_max_type u;
	sint_max_type s;
} int_max_union;

typedef struct {
	int_max_union value;
	bool is_signed;
} int_max_type;

typedef struct {
	int_max_type prev_value;
	// num_size -- number value size in bytes (1,2,4,8)
	// num_size_exp = log2(num_size) (0,1,2,3)
	// num_size = num_sizes[num_size_exp]
	uint_fast8_t num_size_exp;
	uint_fast8_t delta_size_exp;
	bool is_signed;
	bool delta_is_signed;
	bool is_delta;
} field_struct;

ssize_t parse_format(const char *fields_format, field_struct fields[], size_t fields_size) {
	assert(fields_size <= SSIZE_MAX);
	size_t fields_count = 0;
	size_t offset = 0;
	char sign, sep;
	unsigned int size;
	int read_chars, res;
	bool is_reading_delta = false;
	
	while (true) {
		if (fields_count >= fields_size) return -1;
		
		res = sscanf(fields_format+offset, "%c%u%n", &sign, &size, &read_chars);
		if (res != 2) return -1;
		assert(read_chars > 0);
		offset += read_chars;
		
		if (!(sign == SIGNED_CHAR || sign == UNSIGNED_CHAR)) return -1;
		bool is_signed = (sign == SIGNED_CHAR);
		if (size & 7) return -1; // if (size % 8 != 0)
		uint_fast8_t num_size = size / 8;
		uint_fast8_t num_size_exp;
		for (num_size_exp=0; num_size_exp<NUM_SIZES_COUNT && num_sizes[num_size_exp] != num_size; ++num_size_exp);
		if (num_size_exp == NUM_SIZES_COUNT) return -1;
		
		field_struct *field = &fields[fields_count];
		if (! is_reading_delta) {
			field->is_signed = is_signed;
			field->num_size_exp = num_size_exp;
			field->is_delta = false;
		} else {
			field->is_delta = true;
			field->delta_size_exp = num_size_exp;
			field->delta_is_signed = is_signed;
		}
		
		res = sscanf(fields_format+offset, "%c%n", &sep, &read_chars);
		if (res == EOF) {
			++fields_count;
			assert(fields_count > 0 && fields_count <= fields_size);
			return fields_count;
		}
		assert(res == 1);
		assert(read_chars > 0);
		offset += read_chars;
		
		if (! is_reading_delta && sep == 'd') {
			is_reading_delta = true;
		} else if (sep == ',') {
			is_reading_delta = false;
			++fields_count;
		} else {
			return -1;
		}
	}
}

int im_sub(int_max_type *result, const int_max_type *minuend, const int_max_type *subtrahend) {
	assert(minuend->is_signed == subtrahend->is_signed);
	uint_max_type urs;
	bool result_is_neg;
	if (minuend->is_signed) {
		sint_max_type mn = minuend->value.s;
		sint_max_type sb = subtrahend->value.s;
		result_is_neg = (mn < sb);
		if (result_is_neg) {
			sint_max_type tmp = mn; mn = sb; sb = tmp;
		}
		if (mn >= 0 && sb < 0) {
			if (sb == SINT_MAX_TYPE_MIN) {
				if (mn == SINT_MAX_TYPE_MAX) return 1;
				else urs = (uint_max_type)mn + SINT_MAX_TYPE_MIN_ABS;
			} else {
				urs = (uint_max_type)mn + (uint_max_type)(-sb);
			}
		} else {
			urs = mn - sb;
		}
	} else {
		uint_max_type mn = minuend->value.u;
		uint_max_type sb = subtrahend->value.u;
		result_is_neg = (mn < sb);
		urs = (result_is_neg ? sb - mn : mn - sb);
	}
	
	if (result->is_signed) {
		sint_max_type srs;
		if (result_is_neg) {
			if (urs > SINT_MAX_TYPE_MIN_ABS) return 1;
			if (urs == SINT_MAX_TYPE_MIN_ABS) srs = SINT_MAX_TYPE_MIN;
			else srs = -((sint_max_type)urs);
		} else {
			if (urs > SINT_MAX_TYPE_MAX) return 1;
			srs = urs;
		}
		result->value.s = srs;
	} else {
		if (result_is_neg) return 1;
		result->value.u = urs;
	}
	return 0;
}

int im_add_delta(int_max_type *result, const int_max_type *prev, const int_max_type *delta) {
	assert(result->is_signed == prev->is_signed);
	if (prev->is_signed) {
		sint_max_type spv = prev->value.s;
		sint_max_type srs;
		if (delta->is_signed) {
			sint_max_type sdl = delta->value.s;
			if ((sdl < 0 && spv < SINT_MAX_TYPE_MIN - sdl) || (sdl > 0 && spv > SINT_MAX_TYPE_MAX - sdl)) return 1;
			srs = spv + sdl;
		} else {
			uint_max_type udl = delta->value.u;
			if (spv < 0) {
				uint_max_type upv = (spv == SINT_MAX_TYPE_MIN ? SINT_MAX_TYPE_MIN_ABS : (uint_max_type)(-spv));
				// rs = udl - upv
				if (udl < upv) {
					uint_max_type urs = upv - udl;
					if (urs > SINT_MAX_TYPE_MIN_ABS) return 1;
					srs = (urs == SINT_MAX_TYPE_MIN_ABS ? SINT_MAX_TYPE_MIN : -((sint_max_type)urs));
				} else {
					uint_max_type urs = udl - upv;
					if (urs > SINT_MAX_TYPE_MAX) return 1;
					srs = urs;
				}
			} else {
				uint_max_type upv = spv;
				if (upv > ((uint_max_type)SINT_MAX_TYPE_MAX) - udl) return 1;
				srs = upv + udl;
			}
		}
		result->value.s = srs;
	} else {
		uint_max_type upv = prev->value.u;
		uint_max_type urs;
		if (delta->is_signed) {
			sint_max_type sdl = delta->value.s;
			if (sdl < 0) {
				uint_max_type udl = (sdl == SINT_MAX_TYPE_MIN ? SINT_MAX_TYPE_MIN_ABS : (uint_max_type)(-sdl));
				if (upv < udl) return 1;
				urs = upv - udl;
			} else {
				uint_max_type udl = sdl;
				if (upv > UINT_MAX_TYPE_MAX - udl) return 1;
				urs = upv + udl;
			}
		} else {
			uint_max_type udl = delta->value.u;
			if (upv > UINT_MAX_TYPE_MAX - udl) return 1;
			urs = upv + udl;
		}
		result->value.u = urs;
	}
	return 0;
}

int im_fscan(int_max_type *im, FILE *stream) {
	if (im->is_signed) {
		return fscanf(stream, SCN_SINT_MAX_TYPE, &(im->value.s));
	} else {
		return fscanf(stream, SCN_UINT_MAX_TYPE, &(im->value.u));
	}
}

int im_fprint(const int_max_type *im, FILE *stream) {
	if (im->is_signed) {
		return fprintf(stream, PRI_SINT_MAX_TYPE, im->value.s);
	} else {
		return fprintf(stream, PRI_UINT_MAX_TYPE, im->value.u);
	}
}

// bytes -- little endian packed integer
// bytes size == 2^num_size_exp
void im_from_bytes(int_max_type *im, uint_fast8_t num_size_exp, const uint8_t bytes[]) {
	switch (num_size_exp) {
		case 0: {
			uint8_t uv = *((uint8_t *)bytes);
			if (im->is_signed) {
				im->value.s = (int8_t)(*((int8_t *)(&uv)));
			} else {
				im->value.u = uv;
			}
			break;
		}
		case 1: {
			uint16_t uv = le16toh(*((uint16_t *)bytes));
			if (im->is_signed) {
				im->value.s = (int16_t)(*((int16_t *)(&uv)));
			} else {
				im->value.u = uv;
			}
			break;
		}
		case 2: {
			uint32_t uv = le32toh(*((uint32_t *)bytes));
			if (im->is_signed) {
				im->value.s = (int32_t)(*((int32_t *)(&uv)));
			} else {
				im->value.u = uv;
			}
			break;
		}
		case 3: {
			uint64_t uv = le64toh(*((uint64_t *)bytes));
			if (im->is_signed) {
				im->value.s = (int64_t)(*((int64_t *)(&uv)));
			} else {
				im->value.u = uv;
			}
			break;
		}
		default:
			assert(0);
	}
}

// bytes -- little endian packed integer
// bytes size == 2^num_size_exp
int im_to_bytes(const int_max_type *im, uint_fast8_t num_size_exp, uint8_t bytes[]) {
	uint_max_type uv;
	if (im->is_signed) {
		sint_max_type sv = im->value.s;
		uv = *((uint_max_type *)(&sv));
		switch (num_size_exp) {
			case 0:
				if (!(sv >= INT8_MIN  && sv <= INT8_MAX )) return 1;
				break;
			case 1:
				if (!(sv >= INT16_MIN && sv <= INT16_MAX)) return 1;
				break;
			case 2:
				if (!(sv >= INT32_MIN && sv <= INT32_MAX)) return 1;
				break;
			case 3:
				break;
			default:
				assert(0);
		}
	} else {
		uv = im->value.u;
		switch (num_size_exp) {
			case 0:
				if (!(uv <= UINT8_MAX )) return 1;
				break;
			case 1:
				if (!(uv <= UINT16_MAX)) return 1;
				break;
			case 2:
				if (!(uv <= UINT32_MAX)) return 1;
				break;
			case 3:
				break;
			default:
				assert(0);
		}
	}
	
	switch (num_size_exp) {
		case 0:
			*((uint8_t  *)bytes) = (uint8_t)uv;
			break;
		case 1:
			*((uint16_t *)bytes) = htole16((uint16_t)uv);
			break;
		case 2:
			*((uint32_t *)bytes) = htole32((uint32_t)uv);
			break;
		case 3:
			*((uint64_t *)bytes) = htole64((uint64_t)uv);
			break;
		default:
			assert(0);
	}
	return 0;
}

void encode(field_struct fields[], size_t fields_count) {
	if (feof(stdin)) return;
	bool is_first = true;
	while (true) {
		size_t idx;
		for (idx=0; idx<fields_count; ++idx) {
			field_struct *field = &fields[idx];
			
			// input text
			int_max_type im_in;
			im_in.is_signed = field->is_signed;
			int scanf_res =  im_fscan(&im_in, stdin);
			if (scanf_res != 1) {
				if (ferror(stdin)) {
					perror("scanf");
					exit(EXIT_FAILURE);
				}
				if (feof(stdin)) return;
				fprintf(stderr, "Error: wrong input\n");
				exit(EXIT_FAILURE);
			}
			
			// delta
			int_max_type im_out;
			uint_fast8_t im_out_num_size_exp;
			if (field->is_delta && !is_first) {
				im_out.is_signed = field->delta_is_signed;
				int res = im_sub(&im_out, &im_in, &(field->prev_value));
				if (res) {
					fprintf(stderr, "Error: delta overflow\n");
					exit(EXIT_FAILURE);
				}
				im_out_num_size_exp = field->delta_size_exp;
			} else {
				memcpy(&im_out, &im_in, sizeof(im_out));
				im_out_num_size_exp = field->num_size_exp;
			}
			
			// save im_in as previous
			if (field->is_delta) {
				memcpy(&(field->prev_value), &im_in, sizeof(im_in));
			}
			
			// im to bytes
			uint8_t bytes_out[NUM_SIZE_MAX];
			int im_to_bytes_res = im_to_bytes(&im_out, im_out_num_size_exp, bytes_out);
			if (im_to_bytes_res) {
				fprintf(stderr, "Error: output bytes value overflow\n");
				exit(EXIT_FAILURE);
			}
			
			// output bytes
			size_t fwrite_res = fwrite(bytes_out, num_sizes[im_out_num_size_exp], 1, stdout);
			if (fwrite_res != 1) {
				perror("fwrite");
				exit(EXIT_FAILURE);
			}
		}
		if (is_first) is_first = false;
	}
}

void decode(field_struct fields[], size_t fields_count) {
	if (feof(stdin)) return;
	bool is_first = true;
	while (true) {
		size_t idx;
		for (idx=0; idx<fields_count; ++idx) {
			field_struct *field = &fields[idx];
			
			// input bytes
			uint8_t bytes_in[NUM_SIZE_MAX];
			uint_fast8_t im_in_num_size_exp = (field->is_delta && !is_first ? field->delta_size_exp : field->num_size_exp);
			bool im_in_is_signed = (field->is_delta && !is_first ? field->delta_is_signed : field->is_signed);
			size_t fread_res = fread(bytes_in, num_sizes[im_in_num_size_exp], 1, stdin);
			if (fread_res != 1) {
				if (feof(stdin) && ! ferror(stdin)) return;
				perror("fread");
				exit(EXIT_FAILURE);
			}
			
			// bytes to im
			int_max_type im_in;
			im_in.is_signed = im_in_is_signed;
			im_from_bytes(&im_in, im_in_num_size_exp, bytes_in);
			
			// delta
			int_max_type im_out;
			if (field->is_delta && !is_first) {
				im_out.is_signed = field->is_signed;
				int res = im_add_delta(&im_out, &(field->prev_value), &im_in);
				if (res) {
					fprintf(stderr, "Error: delta overflow\n");
					exit(EXIT_FAILURE);
				}
			} else {
				memcpy(&im_out, &im_in, sizeof(im_out));
			}
			
			// save im_out as previous
			if (field->is_delta) {
				memcpy(&(field->prev_value), &im_out, sizeof(im_out));
			}
			
			// output text
			if (im_fprint(&im_out, stdout) < 0)
				goto print_printf_err_and_exit;
			if (idx != fields_count-1) {
				if (printf("\t") < 0) goto print_printf_err_and_exit;
			}
		}
		if (printf("\n") < 0) goto print_printf_err_and_exit;
		if (is_first) is_first = false;
	}
	assert(0);
print_printf_err_and_exit:
	perror("printf");
	exit(EXIT_FAILURE);
}

void fprint_num_sizes(FILE *fout) {
	uint_fast8_t i;
	for (i=0; i<NUM_SIZES_COUNT; ++i) {
		fprintf(fout, "%u", num_sizes[i]*8);
		if (i != NUM_SIZES_COUNT-1) fprintf(fout, "|");
	}
}

void print_help(const char *prog_name) {
	FILE *fout = stderr;
	fprintf(
		fout,
		"Usage:\n"
		"    %s -h\n"
		"    %s (-c|-d) FORMAT\n"
		"Binary encode/decode table of integer numbers"
		"\n"
		"\n"
		"Options:\n"
		"    -h            print this help and exit\n"
		"    -c|-d FORMAT  encode or decode\n"
		"\n"
		"FORMAT describes the way how to encode/decode table fields\n"
		"FORMAT := FIELD[,FIELD,...]\n"
		"FIELD := (SIGN)(SIZE)[DELTA]\n"
		"SIGN := (" SIGNED_STR "|" UNSIGNED_STR ") -- sign indicator: " SIGNED_STR " - signed, " UNSIGNED_STR " - unsigned\n",
		prog_name, prog_name
	);
	fprintf(fout, "SIZE := ("); fprint_num_sizes(fout); fprintf(fout, ") -- size of integer number in bits\n");
	fprintf(
		fout,
		"DELTA := d(SIGN)(SIZE) -- use delta encoding; SIGN and SIZE specify format of delta numbers\n"
		"example FORMAT: '" UNSIGNED_STR "32," SIGNED_STR "64d" UNSIGNED_STR "8'\n"
	);
}

void print_help_and_exit_err(const char *prog_name) {
	print_help(prog_name);
	exit(EXIT_FAILURE);
}

#define FIELDS_COUNT_MAX 65536
static field_struct fields[FIELDS_COUNT_MAX];

int main(int argc, char *argv[]) {
	bool is_encode = false, is_decode = false;
	const char *fields_format = NULL;
	
	static const char optstring[] = "hc:d:";
	while (true) {
		int c = getopt(argc, argv, optstring);
		if (c == -1) break;
		switch (c) {
			case 'h':
				print_help(argv[0]);
				exit(EXIT_SUCCESS);
				break;
			case 'c':
				is_encode = true;
				fields_format = optarg;
				break;
			case 'd':
				is_decode = true;
				fields_format = optarg;
				break;
			default:
				print_help_and_exit_err(argv[0]);
		}
	}
	if (optind < argc || is_encode == is_decode) {
		fprintf(stderr, "Error: wrong arguments\n");
		print_help_and_exit_err(argv[0]);
	}
	
	ssize_t fields_count_or_err = parse_format(fields_format, fields, sizeof(fields)/sizeof(fields[0]));
	if (fields_count_or_err < 0) {
		fprintf(stderr, "Error: wrong FORMAT string\n");
		print_help_and_exit_err(argv[0]);
	}
	size_t fields_count = fields_count_or_err;
	
	if (is_encode) {
		encode(fields, fields_count);
	} else {
		decode(fields, fields_count);
	}
	
	return 0;
}

