// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <glob.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TEXT 256
#define MAX_CHANNELS 128

static bool make_path(char *output, size_t size, const char *directory,
		      const char *attribute)
{
	int written = snprintf(output, size, "%s/%s", directory, attribute);

	return written >= 0 && (size_t)written < size;
}

static bool read_line(const char *path, char *buffer, size_t size)
{
	FILE *file = fopen(path, "r");
	size_t length;

	if (!file)
		return false;
	if (!fgets(buffer, (int)size, file)) {
		fclose(file);
		return false;
	}
	fclose(file);
	length = strlen(buffer);
	while (length && (buffer[length - 1] == '\n' ||
			  buffer[length - 1] == '\r'))
		buffer[--length] = '\0';
	return true;
}

static bool read_millidegree(const char *path, long long *value)
{
	char text[MAX_TEXT], *end;
	long long parsed;

	if (!read_line(path, text, sizeof(text)))
		return false;
	errno = 0;
	parsed = strtoll(text, &end, 10);
	if (errno || *end != '\0')
		return false;
	*value = parsed;
	return true;
}

static void print_temperature(const char *label, long long value)
{
	unsigned long long magnitude;

	if (value < 0)
		magnitude = (unsigned long long)(-(value + 1)) + 1;
	else
		magnitude = (unsigned long long)value;
	printf("  %-30s %s%llu.%03llu C\n", label, value < 0 ? "-" : "",
	       magnitude / 1000, magnitude % 1000);
}

static int dump_hwmon(const char *directory)
{
	char path[PATH_MAX], attribute[64], name[MAX_TEXT], label[MAX_TEXT];
	long long value;
	int channel, shown = 0;

	if (!make_path(path, sizeof(path), directory, "name") ||
	    !read_line(path, name, sizeof(name)) || !strstr(name, "peci"))
		return 0;
	printf("%s name=%s\n", directory, name);
	for (channel = 1; channel <= MAX_CHANNELS; channel++) {
		snprintf(attribute, sizeof(attribute), "temp%d_input", channel);
		if (!make_path(path, sizeof(path), directory, attribute) ||
		    !read_millidegree(path, &value))
			continue;
		snprintf(attribute, sizeof(attribute), "temp%d_label", channel);
		if (!make_path(path, sizeof(path), directory, attribute) ||
		    !read_line(path, label, sizeof(label)))
			snprintf(label, sizeof(label), "temp%d", channel);
		print_temperature(label, value);
		shown++;
	}
	if (!shown)
		puts("  no readable temperature channels");
	return 1;
}

int main(void)
{
	glob_t matches;
	int result, found = 0;
	size_t i;

	result = glob("/sys/class/hwmon/hwmon*", 0, NULL, &matches);
	if (result == GLOB_NOMATCH) {
		fprintf(stderr, "no hwmon devices found\n");
		return 1;
	}
	if (result != 0) {
		fprintf(stderr, "glob failed: %d\n", result);
		return 1;
	}
	for (i = 0; i < matches.gl_pathc; i++)
		found += dump_hwmon(matches.gl_pathv[i]);
	globfree(&matches);
	if (!found) {
		fprintf(stderr, "no PECI hwmon devices found\n");
		return 1;
	}
	return 0;
}

