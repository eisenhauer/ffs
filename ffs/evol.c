#include "config.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <ffs.h>
#include "cercs_env.h"
#include "ffs_internal.h"

#define MAX_DIFF	0xFFFF
#define COMPAT_THRESH	0.8


/* 
 * Compares the two formats.
 * Returns IOformat_order
 * */
static IOformat_order
IOformat_cmp_diff(format1, format2, diff1, diff2)
IOFormat format1;
IOFormat format2;
int *diff1;			/* Number of fields present in format1 and 
				 * not in format2 */
int *diff2;			/* Number of fields present in format2 and 
				 * not in format1 */
{
    IOformat_order tmp_result = Format_Equal;
    IOFieldList orig_field_list1 = field_list_of_IOformat(format1);
    IOFieldList orig_field_list2 = field_list_of_IOformat(format2);
    IOFieldList field_list1, field_list2;
    IOFormat *subformats1 = NULL, *subformats2 = NULL, *tmp_subformats =
	NULL;
    int field_count1, field_count2;
    int i, j, limit;

    /* count fields */
    for (field_count1 = 0; orig_field_list1[field_count1].field_name != NULL;
	 field_count1++);

    /* count fields */
    for (field_count2 = 0; orig_field_list2[field_count2].field_name != NULL;
	 field_count2++);

    field_list1 = malloc(sizeof(field_list1[0]) * (field_count1 + 1));
    field_list2 = malloc(sizeof(field_list2[0]) * (field_count2 + 1));
    memcpy(field_list1, orig_field_list1, 
	   sizeof(field_list1[0]) * (field_count1 + 1));
    memcpy(field_list2, orig_field_list2, 
	   sizeof(field_list2[0]) * (field_count2 + 1));

    qsort(field_list1, field_count1, sizeof(IOField), field_name_compar);
    qsort(field_list2, field_count2, sizeof(IOField), field_name_compar);

    limit = field_count1;
    if (field_count2 < limit)
	limit = field_count2;
    i = j = 0;
    while ((i < field_count1) && (j < field_count2)) {
	int name_cmp;
	if ((name_cmp = strcmp(field_list1[i].field_name,
			       field_list2[j].field_name)) == 0) {
	    /* fields have same name */
	    if (!IO_field_type_eq(field_list1[i].field_type,
				  field_list2[j].field_type)) {
		(*diff1)++;
		(*diff2)++;
	    }
	    i++;
	    j++;
	} else if (name_cmp < 0) {
	    /* name_cmp<0 a field in field_list1 that doesn't appear in 2 */
	    (*diff1)++;
	    i++;
	} else {
	    (*diff2)++;
	    j++;
	}
    }
    (*diff1) += (field_count1 - i);
    (*diff2) += (field_count2 - j);

    /* go through subformats */
    tmp_subformats = subformats1 = get_subformats_IOformat(format1);
    subformats2 = get_subformats_IOformat(format2);
/* TODO: Fix for unmatched subformats 
 * -sandip */
    while (*subformats1 != NULL) {
	char *sub1_name = name_of_IOformat(*subformats1);
	int i = 0;
	if (*subformats1 == format1) {
	    /* self appears in subformat list, skip it */
	    subformats1++;
	    continue;
	}
	while (subformats2[i] != NULL) {
	    if (strcmp(sub1_name, name_of_IOformat(subformats2[i])) == 0) {
		/* same name, compare */
		IOformat_cmp_diff(*subformats1, subformats2[i], diff1,
				  diff2);
		break;
	    }
	    i++;
	}
	subformats1++;
    }

    free(field_list1);
    free(field_list2);
    if (tmp_subformats)
	free(tmp_subformats);
    if (subformats2)
	free(subformats2);
    if (*diff1 == 0) {
	if (*diff2 == 0)
	    tmp_result = Format_Equal;
	else
	    tmp_result = Format_Less;
    } else {
	if (*diff2 == 0)
	    tmp_result = Format_Greater;
	else
	    tmp_result = Format_Incompatible;
    }
    return tmp_result;
}

/* 
 * Not used anywhere yet. But may be useful later.
 * */
static int
count_total_IOfield_list(IOFieldList list, IOFormatList format_list)
{
    int count = 0, i = 0;
    while (list[i].field_name != NULL) {
	char *base_type = base_data_type(list[i].field_type);
	if (str_to_data_type(base_type) == unknown_type) {
	    int j = -1;
	    while (format_list[++j].format_name) {
		if (strcmp(base_type, format_list[j].format_name) == 0) {
		    count +=
			count_total_IOfield_list(format_list[j].field_list,
						 format_list);
		    break;
		}
	    }
	}
	free(base_type);
	count++;
	i++;
    }
    return count;
}

/* 
 * Basically counts the total number of nodes in a data structure tree
 * */
static int
count_total_IOfield(IOFormat format)
{
    int count = 0;
    if (format) {
	int i;
	count = format->body->field_count;
	for (i = 0; i < format->body->field_count; i++)
	    if (format->field_subformats[i] != NULL)
		count += count_total_IOfield(format->field_subformats[i]);
    }
    return count;
}

/* 
 * Implements a simple threshold-based technique to decide whether or not
 * we should do conversion at all when the comparison result between best 
 * possible pair is Format_Incompatible
 * The idea is to keep the number of missing fields below a certain threshold.
 * There is scope of smarter algorithm here.
 * */
static int
check_compat_thresh(IOFormat_Comp_result * comp_result, IOFormat format)
{
    float curr_thresh;
    int field_count = count_total_IOfield(format);

    curr_thresh = (float) comp_result->diff2 / (float) field_count;

    return (curr_thresh < (1.0 - COMPAT_THRESH));
}

/* 
 * Try to find a format in 'formatList' which best matches the 'format'
 * Returns the index of best match found or 
 * -1 if no better comparison (as specified by comp_result) found.
 * comp_result is modified accordingly to indicate the current match purity
 * */
static int
IOformat_list_cmp(IOFormat format, IOFormat *formatList, int listSize,
		  IOFormat_Comp_result * comp_result)
{
    int i, diff1, diff2, nearest_format = -1;
    IOformat_order result = Format_Incompatible;

    for (i = 0; i < listSize; i++) {
	int order;
	if (formatList[i] == NULL)
	    continue;
	order =
	    strcmp(name_of_IOformat(format),
		   name_of_IOformat(formatList[i]));
	if (order < 0)
	    break;
	else if (order > 0)
	    continue;
	diff1 = diff2 = 0;
	result = IOformat_cmp_diff(format, formatList[i], &diff1, &diff2);
	if (result == Format_Equal) {
	    comp_result->diff1 = comp_result->diff2 = 0;
	    nearest_format = i;
	    break;
	}
	if ((diff2 < comp_result->diff2) ||
	    (diff2 == comp_result->diff2 && diff1 < comp_result->diff1)) {
	    comp_result->diff1 = diff1;
	    comp_result->diff2 = diff2;
	    nearest_format = i;
	}
    }
    return nearest_format;
}

static int
IOformat_list_cmp2(IOFormat format, IOFormat *formatList, int listSize,
		  IOFormat_Comp_result * comp_result)
{
    int i, diff1, diff2, nearest_format = -1;
    IOformat_order result = Format_Incompatible;

    for (i = 0; i < listSize; i++) {
	if (formatList[i] == NULL)
	    continue;
	diff1 = diff2 = 0;
	result = IOformat_cmp_diff(format, formatList[i], &diff1, &diff2);
	if (result == Format_Equal) {
	    comp_result->diff1 = comp_result->diff2 = 0;
	    nearest_format = i;
	    break;
	}
	if ((diff2 < comp_result->diff2) ||
	    (diff2 == comp_result->diff2 && diff1 < comp_result->diff1)) {
	    comp_result->diff1 = diff1;
	    comp_result->diff2 = diff2;
	    nearest_format = i;
	}
    }
    return nearest_format;
}

/* 
 * First check whether 'format' matches exactly with any of 'formatList'
 * If exact match not found, try to match with compatible formats (formats 
 * that 'format' can be translated to)
 * Returns the index of best match found or 
 * -1 if no better comparison (as specified by comp_result) found.
 * */
extern int
IOformat_compat_cmp(IOFormat format, IOFormat *formatList,
		    int listSize, IOcompat_formats * older_format)
{
    IOFormat prior_format;
    int i = 0, nearest_format = -1;
    IOcompat_formats compats;
    IOFormat_Comp_result comp_result = { MAX_DIFF, MAX_DIFF };

    *older_format = NULL;
    nearest_format =
	IOformat_list_cmp(format, formatList, listSize, &comp_result);
    if (nearest_format != -1 && !comp_result.diff1 && !comp_result.diff2)
	return nearest_format;

    compats = get_compat_formats(format);
    if (compats == NULL)
	return -1;
    while ((prior_format = compats[i].prior_format)) {
	int tmp = IOformat_list_cmp(prior_format, formatList, listSize,
				    &comp_result);
	if (tmp != -1) {
	    nearest_format = tmp;
	    *older_format = &compats[i];
	}
	if (comp_result.diff1 == 0 && comp_result.diff2 == 0)
	    break;
	i++;
    }
    if (nearest_format != -1 &&
	!check_compat_thresh(&comp_result, formatList[nearest_format])){
	nearest_format = -1;
	*older_format = NULL;
    }
    return nearest_format;
}

/* 
 * First check whether 'format' matches exactly with any of 'formatList'
 * If exact match not found, try to match with compatible formats (formats 
 * that 'format' can be translated to)
 * Returns the index of best match found or 
 * -1 if no better comparison (as specified by comp_result) found.
 * */
extern int
IOformat_compat_cmp2(IOFormat format, IOFormat *formatList,
		    int listSize, IOcompat_formats * older_format)
{
    IOFormat prior_format;
    int i = 0, nearest_format = -1;
    IOcompat_formats compats;
    IOFormat_Comp_result saved_comp_result = { MAX_DIFF, MAX_DIFF };

    *older_format = NULL;
    nearest_format =
	IOformat_list_cmp2(format, formatList, listSize, &saved_comp_result);
    if (nearest_format != -1 && !saved_comp_result.diff1 && 
	!saved_comp_result.diff2) {
	/* exact match */
	return nearest_format;
    }

    compats = get_compat_formats(format);
    if (compats == NULL) {
	if (!saved_comp_result.diff2) {
	    return nearest_format;
	}
	return -1;
    }
    while ((prior_format = compats[i].prior_format)) {
	IOFormat_Comp_result comp_result={MAX_DIFF, MAX_DIFF};

	int tmp = IOformat_list_cmp2(prior_format, formatList, listSize,
				    &comp_result);
	if (tmp != -1) {
	    if (saved_comp_result.diff1 > comp_result.diff1) {
		nearest_format = tmp;
		*older_format = &compats[i];
		saved_comp_result = comp_result;
	    }
	}
	if (comp_result.diff1 == 0 && comp_result.diff2 == 0)
	    break;
	i++;
    }
    if (nearest_format != -1 &&
	!check_compat_thresh(&saved_comp_result, formatList[nearest_format])){
	nearest_format = -1;
	*older_format = NULL;
    }
    return nearest_format;
}

/**
 * Localize the "format" and set the conversion context to convert the wire
 * format to this localized format.
 * Return the localized format list
 */
extern IOFormatList
IOlocalize_conv(IOContext context, IOFormat format)
{
    IOFormatList local_format_list = NULL;
    IOFormat *wire_subformats = get_subformats_IOformat(format);
    int i = 0;

    while (wire_subformats[i] != NULL) {
	int local_struct_size;
	IOFieldList wire_field_list =
	    field_list_of_IOformat(wire_subformats[i]);
	IOFieldList local_field_list = copy_field_list(wire_field_list);

	/* 
	 * determine an appropriate native layout for this structure
	 * and set the conversion
	 */
	local_field_list = localize_field_list(local_field_list, context);
	local_struct_size =
	    struct_size_field_list(local_field_list, sizeof(char *));
	set_conversion_IOcontext(context, wire_subformats[i],
				 local_field_list, local_struct_size);
	local_format_list =
	    realloc(local_format_list,
		    sizeof(local_format_list[0]) * (i + 2));
	local_format_list[i].format_name =
	    strdup(name_of_IOformat(wire_subformats[i]));

	local_format_list[i].field_list = local_field_list;
	i++;
    }
    local_format_list[i].format_name = NULL;
    free(wire_subformats);
    return local_format_list;
}

/**
 * Localize the "format".
 * Register this localized format to the "context".
 * Set the conversion context to convert this localized format to native 
 * format as specified by native_field_list and native_subformat_list.
 * Return the localized format list.
 */
extern IOFormatList
IOlocalize_register_conv(IOContext context, IOFormat format,
			 IOFieldList native_field_list,
			 IOFormatList native_subformat_list,
			 IOFormat *local_prior_format,
			 int *local_struct_size_out)
{
    IOFormatList local_f1_formats = NULL;
    IOFormat *wire_subformats = get_subformats_IOformat(format);
    int i = 0, native_struct_size;

    native_struct_size = struct_size_field_list(native_field_list,
						sizeof(char *));

#ifdef NOTDEF
    while (wire_subformats[i] != NULL) {
	int local_struct_size;
	IOFieldList wire_field_list =
	    field_list_of_IOformat(wire_subformats[i]);
	IOFieldList local_field_list = copy_field_list(wire_field_list);
	char *subformat_name = name_of_IOformat(wire_subformats[i]);
	int j = 0;

	/* 
	 * determine an appropriate native layout for this structure
	 * and set the conversion
	 */
	local_field_list = localize_field_list(local_field_list, context);
	local_struct_size =
	    struct_size_field_list(local_field_list, sizeof(char *));

	/* We don't need the subformats. The last one will be the top
	 * level IOFormat */

	*local_prior_format =
	    register_IOcontext_format(subformat_name, local_field_list,
				      context);
	while (native_subformat_list
	       && (native_subformat_list[j].format_name != NULL)) {
	    if (strcmp
		(native_subformat_list[j].format_name,
		 subformat_name) == 0) {
		IOFieldList sub_field_list;
		int sub_struct_size;
		sub_field_list = native_subformat_list[j].field_list;
		sub_struct_size = struct_size_field_list(sub_field_list,
							 sizeof(char *));
		set_conversion_IOcontext(context,
					 *local_prior_format,
					 sub_field_list, sub_struct_size);
		break;
	    }
	    j++;
	}

	local_f1_formats = realloc(local_f1_formats,
				   sizeof(local_f1_formats[0]) * (i + 2));
	local_f1_formats[i].format_name =
	    strdup(name_of_IOformat(wire_subformats[i]));
	local_f1_formats[i].field_list = local_field_list;
	if (wire_subformats[i + 1] == NULL) {
	    /* last one */
	    *local_struct_size_out = local_struct_size;
	    local_f1_formats[i + 1].format_name = NULL;
	}
	i++;
    }
#endif
    set_conversion_IOcontext(context, *local_prior_format,
			     native_field_list, native_struct_size);
    free(wire_subformats);
    return local_f1_formats;
}
