#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h> // getopt, ssize_t
#include <limits.h> // SSIZE_MAX

#define UNSIGNED_STR "u"
#define UNSIGNED_CHAR 'u'
#define SIGNED_STR "s"
#define SIGNED_CHAR 's'

#define NUM_SIZES_COUNT 4
const uint_fast8_t num_sizes[NUM_SIZES_COUNT] = {1, 2, 4, 8};
#define NUM_SIZE_MAX 8

typedef struct {
	uint8_t prev_num_buffer[NUM_SIZE_MAX];
	// num_size -- number value size in bytes (1,2,4,8)
	// num_size_exp = log2(num_size) (0,1,2,3)
	// num_size = num_sizes[num_size_exp]
	uint_fast8_t num_size_exp;
	uint_fast8_t delta_size_exp;
	bool is_signed;
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
		if (size & 7) return -1; // if (size % 8 != 0)
		uint_fast8_t num_size = size / 8;
		uint_fast8_t num_size_exp;
		for (num_size_exp=0; num_size_exp<NUM_SIZES_COUNT && num_sizes[num_size_exp] != num_size; ++num_size_exp);
		if (num_size_exp == NUM_SIZES_COUNT) return -1;
		
		field_struct *field = &fields[fields_count];
		if (! is_reading_delta) {
			field->is_signed = (sign == SIGNED_CHAR);
			field->num_size_exp = num_size_exp;
			field->is_delta = false;
			++fields_count;
		} else {
			field->is_delta = true;
			field->delta_size_exp = num_size_exp;
		}
		
		res = sscanf(fields_format+offset, "%c%n", &sep, &read_chars);
		if (res == EOF) {
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
		} else {
			return -1;
		}
	}
}

typedef union {
	//uint8_t bytes[NUM_SIZE_MAX];
	uint64_t uint64;
	 int64_t sint64;
	uint32_t uint32;
	 int32_t sint32;
	uint16_t uint16;
	 int16_t sint16;
	uint8_t uint8;
	 int8_t sint8;
} number_value_union;

// TODO: endianness
int fscan_number(FILE *stream, bool is_signed, uint_fast8_t num_size_exp, number_value_union *buffer) {
	if (is_signed) {
		switch (num_size_exp) {
			case 0: return fscanf(stream, "%" SCNd8,  &(buffer->sint8));
			case 1: return fscanf(stream, "%" SCNd16, &(buffer->sint16));
			case 2: return fscanf(stream, "%" SCNd32, &(buffer->sint32));
			case 3: return fscanf(stream, "%" SCNd64, &(buffer->sint64));
			default: assert(0);
		}
	} else {
		switch (num_size_exp) {
			case 0: return fscanf(stream, "%" SCNu8,  &(buffer->uint8));
			case 1: return fscanf(stream, "%" SCNu16, &(buffer->uint16));
			case 2: return fscanf(stream, "%" SCNu32, &(buffer->uint32));
			case 3: return fscanf(stream, "%" SCNu64, &(buffer->uint64));
			default: assert(0);
		}
	}
}

// TODO: endianness
int fprint_number(FILE *stream, bool is_signed, uint_fast8_t num_size_exp, const number_value_union *buffer) {
	if (is_signed) {
		switch (num_size_exp) {
			case 0: return fprintf(stream, "%" PRId8,  buffer->sint8);
			case 1: return fprintf(stream, "%" PRId16, buffer->sint16);
			case 2: return fprintf(stream, "%" PRId32, buffer->sint32);
			case 3: return fprintf(stream, "%" PRId64, buffer->sint64);
			default: assert(0);
		}
	} else {
		switch (num_size_exp) {
			case 0: return fprintf(stream, "%" PRIu8,  buffer->uint8);
			case 1: return fprintf(stream, "%" PRIu16, buffer->uint16);
			case 2: return fprintf(stream, "%" PRIu32, buffer->uint32);
			case 3: return fprintf(stream, "%" PRIu64, buffer->uint64);
			default: assert(0);
		}
	}
}

void encode(field_struct fields[], size_t fields_count) {
	if (feof(stdin)) return;
	bool is_first = true;
	while (true) {
		size_t idx;
		for (idx=0; idx<fields_count; ++idx) {
			// TODO: delta
			number_value_union num_buffer;
			field_struct *field = &fields[idx];
			int scanf_res = fscan_number(stdin, field->is_signed, field->num_size_exp, &num_buffer);
			if (scanf_res != 1) {
				if (ferror(stdin)) {
					perror("scanf");
					exit(EXIT_FAILURE);
				}
				if (feof(stdin)) return;
				fprintf(stderr, "Error: wrong input\n");
				exit(EXIT_FAILURE);
			}
			size_t fwrite_res = fwrite(num_buffer.bytes, num_sizes[field->num_size_exp], 1, stdout);
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
			// TODO: delta
			number_value_union num_buffer;
			field_struct *field = &fields[idx];
			size_t fread_res = fread(num_buffer.bytes, num_sizes[field->num_size_exp], 1, stdin);
			if (fread_res != 1) {
				if (feof(stdin) && ! ferror(stdin)) return;
				perror("fread");
				exit(EXIT_FAILURE);
			}
			if (fprint_number(stdout, field->is_signed, field->num_size_exp, &num_buffer) < 0)
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
		"Binary encode/decode table of unsigned integer numbers"
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

