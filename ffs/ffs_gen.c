#include "config.h"
#include "stdio.h"
#include "stdlib.h"
#include "ffs.h"

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include "assert.h"
#include "ffs_internal.h"
#ifdef HAVE_DRISC_H
#include "drisc.h"
#define static_ctx c 
#define _vrr(x) x
#else
#include "vcode.h"
#include "drisc_vcode.h"
typedef void *drisc_ctx;
#endif
#include "ffs_gen.h"

/* #define REG_DEBUG(x) printf x ; */
#define REG_DEBUG(x)
#define gen_fatal(str) do {fprintf(stderr, "%s\n", str); exit(0);} while (0)

iogen_oprnd
gen_operand(src_reg, offset, size, data_type, aligned, byte_swap)
dr_reg_t src_reg;
int offset;
int size;
FMdata_type data_type;
int aligned;
int byte_swap;
{
    iogen_oprnd ret_val;
    ret_val.address = 1;
    ret_val.size = size;
    ret_val.data_type = data_type;
    ret_val.offset = offset;
    ret_val.aligned = aligned;
    ret_val.byte_swap = byte_swap;
    ret_val.vc_reg = src_reg;
    ret_val.vc_reg2 = src_reg;
    return ret_val;
}

void
gen_load(c, src_oprnd)
drisc_ctx c;
iogen_oprnd_ptr src_oprnd;
{
    iogen_oprnd tmp_val;
    tmp_val = gen_fetch(c, 
			src_oprnd->vc_reg, src_oprnd->offset, src_oprnd->size,
			src_oprnd->data_type, src_oprnd->aligned,
			src_oprnd->byte_swap);
    *src_oprnd = tmp_val;
}

iogen_oprnd
gen_bswap_fetch(c, src_reg, offset, size, data_type, aligned)
drisc_ctx c;
dr_reg_t src_reg;
int offset;
int size;
FMdata_type data_type;
int aligned;
{
    iogen_oprnd ret_val;
    ret_val.address = 0;
    ret_val.size = size;
    ret_val.data_type = data_type;
    ret_val.offset = 0;
    ret_val.aligned = 0;
    ret_val.byte_swap = 0;
    switch (data_type) {
    case unknown_type:
	assert(FALSE);
	break;
    case integer_type:
    case boolean_type:
    case enumeration_type:
	switch (size) {
	case 1:		/* sizeof char */
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_C, DR_TEMP))
		gen_fatal("gen fetch out of registers \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    dr_ldbsci(c, ret_val.vc_reg, src_reg, offset);
	    break;
	case 2:		/* sizeof short */
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_S, DR_TEMP))
		gen_fatal("gen fetch out of registers \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    dr_ldbssi(c, ret_val.vc_reg, src_reg, offset);
	    break;
	case 4:		/* sizeof int */
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_I, DR_TEMP))
		gen_fatal("gen fetch out of registers A\n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    dr_ldbsii(c, ret_val.vc_reg, src_reg, offset);
	    break;
#if SIZEOF_LONG != 4
	case SIZEOF_LONG:
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_L, DR_TEMP))
		gen_fatal("gen fetch out of registers \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    if (!aligned || ((offset & 0x7) != 0)) {
		/* misaligned */
		if ((offset & 0x3) == 0) {
		    /* 4 byte aligned */
		    dr_reg_t high_reg;
		    if (!dr_getreg(c, &high_reg, DR_L, DR_TEMP))
			gen_fatal("gen fetch out of registers \n");
#ifdef WORDS_BIGENDIAN
		    /* vc_reg2 holds high value */
		    dr_ldbsui(c, ret_val.vc_reg, src_reg, offset);
		    dr_ldbsii(c, high_reg, src_reg, offset + 4);
#else
		    dr_ldbsii(c, high_reg, src_reg, offset);
		    dr_ldbsui(c, ret_val.vc_reg, src_reg, offset + 4);
#endif
		    dr_lshli(c, high_reg, high_reg, 32);
		    dr_orl(c, ret_val.vc_reg, high_reg, ret_val.vc_reg);
		    dr_putreg(c, high_reg, DR_L);
		} else {
		    assert(FALSE);
		}
	    } else {
		dr_ldbsli(c, ret_val.vc_reg, src_reg, offset);
	    }
	    break;
#else
	case 8:
	    /* simulate with double reg */
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_I, DR_TEMP) ||
		!dr_getreg(c, &ret_val.vc_reg2, DR_I, DR_TEMP))
		gen_fatal("gen fetch out of registers B \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg2)));

#ifdef WORDS_BIGENDIAN
	    /* vc_reg2 holds high value */
	    dr_ldbsii(c, ret_val.vc_reg, src_reg, offset);
	    dr_ldbsii(c, ret_val.vc_reg2, src_reg, offset + 4);
#else
	    dr_ldbsii(c, ret_val.vc_reg2, src_reg, offset);
	    dr_ldbsii(c, ret_val.vc_reg, src_reg, offset + 4);
#endif
	    break;
#endif
	}
	break;
    case unsigned_type:
    case char_type:
	switch (size) {
	case 1:		/* sizeof char */
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_UC, DR_TEMP))
		gen_fatal("gen fetch out of registers \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    dr_ldbsuci(c, ret_val.vc_reg, src_reg, offset);
	    break;
	case 2:		/* sizeof short */
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_US, DR_TEMP))
		gen_fatal("gen fetch out of registers \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    dr_ldbsusi(c, ret_val.vc_reg, src_reg, offset);
	    break;
	case 4:		/* sizeof int */
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_U, DR_TEMP))
		gen_fatal("gen fetch out of registers \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    dr_ldbsui(c, ret_val.vc_reg, src_reg, offset);
	    break;
#if SIZEOF_LONG != 4
	case SIZEOF_LONG:
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_UL, DR_TEMP))
		gen_fatal("gen fetch out of registers \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    dr_ldbsuli(c, ret_val.vc_reg, src_reg, offset);
	    break;
#else
	case 8:
	    /* simulate with double reg */
	    if (!dr_getreg(c, &ret_val.vc_reg2, DR_U, DR_TEMP) ||
		!dr_getreg(c, &ret_val.vc_reg, DR_U, DR_TEMP))
		gen_fatal("gen fetch out of registers \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg2)));
	    /* vc_reg2 holds high value */
#ifdef WORDS_BIGENDIAN
	    dr_ldbsui(c, ret_val.vc_reg, src_reg, offset);
	    dr_ldbsui(c, ret_val.vc_reg2, src_reg, offset + 4);
#else
	    dr_ldbsui(c, ret_val.vc_reg2, src_reg, offset);
	    dr_ldbsui(c, ret_val.vc_reg, src_reg, offset + 4);
#endif
	    break;
#endif
	}
	break;
    case float_type:
    case string_type:
	assert(FALSE);
    }
    return ret_val;
}

iogen_oprnd
gen_set(drisc_ctx c, int size, char* value)
{
    iogen_oprnd ret_val;
    ret_val.address = 0;
    ret_val.size = size;
    ret_val.data_type = integer_type;
    ret_val.offset = 0;
    ret_val.aligned = 0;
    ret_val.byte_swap = 0;

    switch (size) {
    case 1:		/* sizeof char */
	if (!dr_getreg(c, &ret_val.vc_reg, DR_C, DR_TEMP))
	    gen_fatal("gen fetch out of registers \n");
	REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	dr_setc(c, ret_val.vc_reg, value[0]);
	break;
    case 2:		/* sizeof short */
	if (!dr_getreg(c, &ret_val.vc_reg, DR_S, DR_TEMP))
	    gen_fatal("gen fetch out of registers \n");
	REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	dr_sets(c, ret_val.vc_reg, *((short*)value));
	break;
    case 4:		/* sizeof int */
	if (!dr_getreg(c, &ret_val.vc_reg, DR_I, DR_TEMP))
	    gen_fatal("gen fetch out of registers C\n");
	REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	dr_seti(c, ret_val.vc_reg, *((int*)value));
	break;
#if SIZEOF_LONG != 4
    case SIZEOF_LONG:
	if (!dr_getreg(c, &ret_val.vc_reg, DR_L, DR_TEMP))
	    gen_fatal("gen fetch out of registers \n");
	REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	dr_setl(c, ret_val.vc_reg, *((long*)value));
	break;
#else
    case 8:
	/* simulate with double reg */
	if (!dr_getreg(c, &ret_val.vc_reg, DR_I, DR_TEMP) ||
	    !dr_getreg(c, &ret_val.vc_reg2, DR_I, DR_TEMP))
	    gen_fatal("gen fetch out of registers D \n");
	REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg2)));
	/* vc_reg2 holds high value */
#ifdef WORDS_BIGENDIAN
	dr_seti(c, ret_val.vc_reg2, *((long*)value));
	dr_seti(c, ret_val.vc_reg, *((long*)(value + 4)));
#else
	dr_seti(c, ret_val.vc_reg, *((long*)value));
	dr_seti(c, ret_val.vc_reg2, *((long*)(value + 4)));
#endif
	break;
#endif
    }
    return ret_val;
}
   
iogen_oprnd
gen_fetch(c, src_reg, offset, size, data_type, aligned, byte_swap)
drisc_ctx c;
dr_reg_t src_reg;
int offset;
int size;
FMdata_type data_type;
int aligned;
int byte_swap;
{
    iogen_oprnd ret_val;

#if defined(v_ldbsi) || defined(HAVE_DRISC_H)
    if (dr_has_ldbs(c)) {
	/* have byte swap load extension */
	if (byte_swap && (data_type != float_type)) {
	    return gen_bswap_fetch(c, src_reg, offset, size, data_type, 
				   aligned);
	}
    }
#endif
    ret_val.address = 0;
    ret_val.size = size;
    ret_val.data_type = data_type;
    ret_val.offset = 0;
    ret_val.aligned = 0;
    ret_val.byte_swap = 0;
    switch (data_type) {
    case unknown_type:
	assert(FALSE);
	break;
    case integer_type:
    case boolean_type:
    case enumeration_type:
	switch (size) {
	case 1:		/* sizeof char */
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_C, DR_TEMP))
		gen_fatal("gen fetch out of registers \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    dr_ldci(c, ret_val.vc_reg, src_reg, offset);
	    break;
	case 2:		/* sizeof short */
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_S, DR_TEMP))
		gen_fatal("gen fetch out of registers \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    dr_ldsi(c, ret_val.vc_reg, src_reg, offset);
	    break;
	case 4:		/* sizeof int */
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_I, DR_TEMP))
		gen_fatal("gen fetch out of registers C\n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    dr_ldii(c, ret_val.vc_reg, src_reg, offset);
	    break;
#if SIZEOF_LONG != 4
	case SIZEOF_LONG:
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_L, DR_TEMP))
		gen_fatal("gen fetch out of registers \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    dr_ldli(c, ret_val.vc_reg, src_reg, offset);
	    break;
#else
	case 8:
	    /* simulate with double reg */
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_I, DR_TEMP) ||
		!dr_getreg(c, &ret_val.vc_reg2, DR_I, DR_TEMP))
		gen_fatal("gen fetch out of registers D \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg2)));
	    /* vc_reg2 holds high value */
#ifdef WORDS_BIGENDIAN
	    dr_ldii(c, ret_val.vc_reg2, src_reg, offset);
	    dr_ldii(c, ret_val.vc_reg, src_reg, offset + 4);
#else
	    dr_ldii(c, ret_val.vc_reg, src_reg, offset);
	    dr_ldii(c, ret_val.vc_reg2, src_reg, offset + 4);
#endif
	    break;
#endif
	}
	break;
    case unsigned_type:
    case char_type:
	switch (size) {
	case 1:		/* sizeof char */
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_UC, DR_TEMP))
		gen_fatal("gen fetch out of registers \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    dr_lduci(c, ret_val.vc_reg, src_reg, offset);
	    break;
	case 2:		/* sizeof short */
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_US, DR_TEMP))
		gen_fatal("gen fetch out of registers \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    dr_ldusi(c, ret_val.vc_reg, src_reg, offset);
	    break;
	case 4:		/* sizeof int */
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_U, DR_TEMP))
		gen_fatal("gen fetch out of registers \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    dr_ldui(c, ret_val.vc_reg, src_reg, offset);
	    break;
#if SIZEOF_LONG != 4
	case SIZEOF_LONG:
	    if (!dr_getreg(c, &ret_val.vc_reg, DR_UL, DR_TEMP))
		gen_fatal("gen fetch out of registers \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    dr_lduli(c, ret_val.vc_reg, src_reg, offset);
	    break;
#else
	case 8:
	    /* simulate with double reg */
	    if (!dr_getreg(c, &ret_val.vc_reg2, DR_U, DR_TEMP) ||
		!dr_getreg(c, &ret_val.vc_reg, DR_U, DR_TEMP))
		gen_fatal("gen fetch out of registers \n");
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
	    REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg2)));
	    /* vc_reg2 holds high value */
#ifdef WORDS_BIGENDIAN
	    dr_ldui(c, ret_val.vc_reg2, src_reg, offset);
	    dr_ldui(c, ret_val.vc_reg, src_reg, offset + 4);
#else
	    dr_ldui(c, ret_val.vc_reg, src_reg, offset);
	    dr_ldui(c, ret_val.vc_reg2, src_reg, offset + 4);
#endif
	    break;
#endif
	}
	break;
    case float_type:
	if (byte_swap) {
	    /* best to byte_swap floating point things as we're loading */
	    ret_val.offset = offset;
	    ret_val.aligned = aligned;
	    ret_val.address = TRUE;
	    ret_val.vc_reg = src_reg;
	    ret_val.byte_swap = TRUE;
	    gen_byte_swap(c, &ret_val);
	    byte_swap = FALSE;	/* taken care of */
	} else {
	    switch (size) {
	    case SIZEOF_FLOAT:	/* sizeof char */
		dr_getreg(c, &ret_val.vc_reg, DR_F, DR_TEMP);
		REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
		dr_ldfi(c, ret_val.vc_reg, src_reg, offset);
		break;
	    case SIZEOF_DOUBLE:	/* sizeof short */
		dr_getreg(c, &ret_val.vc_reg, DR_D, DR_TEMP);
		REG_DEBUG(("get %d in gen_Fetch\n", _vrr(ret_val.vc_reg)));
		dr_lddi(c, ret_val.vc_reg, src_reg, offset);
		break;
	    }
	}
	break;
    case string_type:
	assert(FALSE);
    }
    if (byte_swap) {
	gen_byte_swap(c, &ret_val);
    }
    return ret_val;
}

void
gen_byte_swap(c, src_oprnd)
drisc_ctx c;
iogen_oprnd_ptr src_oprnd;
{
    iogen_oprnd swap_oprnd;
    if (src_oprnd->address) {
	if (src_oprnd->data_type == float_type) {
	    swap_oprnd = *src_oprnd;
	    swap_oprnd.data_type = integer_type;
	    gen_load(c, &swap_oprnd);
	    /* gen_load did everything for us */
	    goto do_float_load_store;
	} else {
	    swap_oprnd = *src_oprnd;
	    gen_load(c, &swap_oprnd);
#if defined(v_ldbsi) || defined(HAVE_DRISC_H)
	    if (dr_has_ldbs(c)) {
		/* gen_load did everything for us */
		return;
	    }
#endif
	}
    } else {
	assert(src_oprnd->data_type != float_type);
	swap_oprnd = *src_oprnd;
    }
    /* no byte swap load extension, do it manually */
    switch (swap_oprnd.size) {
    case 1:
	break;
    case 2:
	dr_bswaps(c, swap_oprnd.vc_reg, swap_oprnd.vc_reg);
	break;
    case 4:
	dr_bswapi(c, swap_oprnd.vc_reg, swap_oprnd.vc_reg);
	break;
    case 8:
	if (sizeof(long) == 4) {
	    /* swap top and bottom */
	    dr_reg_t tmp_reg = swap_oprnd.vc_reg;
	    swap_oprnd.vc_reg = swap_oprnd.vc_reg2;
	    swap_oprnd.vc_reg2 = tmp_reg;
	    /* then byte swap each */
	    dr_bswapi(c, swap_oprnd.vc_reg, swap_oprnd.vc_reg);
	    dr_bswapi(c, swap_oprnd.vc_reg2, swap_oprnd.vc_reg2);
	} else {
	    dr_bswapl(c, swap_oprnd.vc_reg, swap_oprnd.vc_reg);
	}
	break;
    default:
	assert(FALSE);
    }
 do_float_load_store:
    if (src_oprnd->address) {
	if (src_oprnd->data_type == float_type) {
	    /* lose type info by storing to memory and retrieving */
	    int tmp_base = dr_localb(c, src_oprnd->size);
	    gen_store(c, swap_oprnd, dr_lp(c), tmp_base, src_oprnd->size,
		      integer_type, TRUE);
	    free_oprnd(c, swap_oprnd);
	    swap_oprnd = gen_fetch(c, dr_lp(c), tmp_base, src_oprnd->size,
				   src_oprnd->data_type, TRUE, FALSE);
	}
    }
    *src_oprnd = swap_oprnd;
}

void
gen_store(c, src, dest_reg, offset, size, data_type, aligned)
drisc_ctx c;
iogen_oprnd src;
dr_reg_t dest_reg;
int offset;
int size;
FMdata_type data_type;
int aligned;
{
    assert(src.size == size);

    switch (data_type) {
    case unknown_type:
	assert(FALSE);
	break;
    case integer_type:
    case boolean_type:
    case enumeration_type:
	switch (size) {
	case 1:		/* sizeof char */
	    dr_stci(c, src.vc_reg, dest_reg, offset);
	    break;
	case 2:		/* sizeof short */
	    dr_stsi(c, src.vc_reg, dest_reg, offset);
	    break;
	case 4:		/* sizeof int */
	    dr_stii(c, src.vc_reg, dest_reg, offset);
	    break;
#if SIZEOF_LONG != 4
	case SIZEOF_LONG:
	    dr_stli(c, src.vc_reg, dest_reg, offset);
	    break;
#else
	case 8:
	    /* simulate with double reg */
	    /* vc_reg2 holds high value */
#ifdef WORDS_BIGENDIAN
	    dr_stii(c, src.vc_reg2, dest_reg, offset);
	    dr_stii(c, src.vc_reg, dest_reg, offset + 4);
#else
	    dr_stii(c, src.vc_reg, dest_reg, offset);
	    dr_stii(c, src.vc_reg2, dest_reg, offset + 4);
#endif
	    break;
#endif
	}
	break;
    case unsigned_type:
    case char_type:
	switch (size) {
	case 1:		/* sizeof char */
	    dr_stuci(c, src.vc_reg, dest_reg, offset);
	    break;
	case 2:		/* sizeof short */
	    dr_stusi(c, src.vc_reg, dest_reg, offset);
	    break;
	case 4:		/* sizeof int */
	    dr_stui(c, src.vc_reg, dest_reg, offset);
	    break;
#if SIZEOF_LONG != 4
	case SIZEOF_LONG:
	    dr_stuli(c, src.vc_reg, dest_reg, offset);
	    break;
#else
	case 8:
	    /* simulate with double reg */
	    /* vc_reg2 holds high value */
#ifdef WORDS_BIGENDIAN
	    dr_stui(c, src.vc_reg2, dest_reg, offset);
	    dr_stui(c, src.vc_reg, dest_reg, offset + 4);
#else
	    dr_stui(c, src.vc_reg, dest_reg, offset);
	    dr_stui(c, src.vc_reg2, dest_reg, offset + 4);
#endif
	    break;
#endif
	}
	break;
    case float_type:
	switch (size) {
	case SIZEOF_FLOAT:	/* sizeof char */
	    dr_stfi(c, src.vc_reg, dest_reg, offset);
	    break;
	case SIZEOF_DOUBLE:	/* sizeof short */
	    dr_stdi(c, src.vc_reg, dest_reg, offset);
	    break;
	}
	break;
    case string_type:

	break;
    }
}

void
gen_memcpy(c, src, src_offset, dest, dest_offset, size, const_size)
drisc_ctx c;
dr_reg_t src;
int src_offset;
dr_reg_t dest;
int dest_offset;
dr_reg_t size;
int const_size;
{
    dr_reg_t final_src, final_dest;
    if (src_offset != 0) {
	if (dr_getreg(c, &final_src, DR_P, DR_TEMP) == 0)
	    gen_fatal("gen memcpy convert out of registers \n");
	dr_addpi(c, final_src, src, src_offset);
    } else {
	final_src = src;
    }
    if (dest_offset != 0) {
	if (dr_getreg(c, &final_dest, DR_P, DR_TEMP) == 0)
	    gen_fatal("gen memcpy convert out of registers \n");
	dr_addpi(c, final_dest, dest, dest_offset);
    } else {
	final_dest = dest;
    }
    if (const_size != 0) {
#ifdef HAVE_DRISC_H
	dr_scalli(c, (void*) memcpy, "%p%p%I", final_dest, final_src, 
		  const_size);
#else
	v_scalli((v_iptr) memcpy, "%p%p%I", final_dest, final_src, const_size);
#endif
    } else {
#ifdef HAVE_DRISC_H
	dr_scalli(c, (void*) memcpy, "%p%p%i", final_dest, final_src, size);
#else
	v_scalli((v_iptr) memcpy, "%p%p%i", final_dest, final_src, size);
#endif
    }
    if (src_offset != 0) {
	dr_putreg(c, final_src, DR_P);
    }
    if (dest_offset != 0) {
	dr_putreg(c, final_dest, DR_P);
    }
}

void
free_oprnd(c, oprnd)
drisc_ctx c;
iogen_oprnd oprnd;
{
    REG_DEBUG(("put %d in free\n", _vrr(oprnd.vc_reg)));
    switch (oprnd.data_type) {
    case unknown_type:
	assert(FALSE);
	break;
    case integer_type:
    case boolean_type:
    case enumeration_type:
	switch (oprnd.size) {
	case 1:		/* sizeof char */
	    dr_putreg(c, oprnd.vc_reg, DR_C);
	    break;
	case 2:		/* sizeof short */
	    dr_putreg(c, oprnd.vc_reg, DR_S);
	    break;
	case 4:		/* sizeof int */
	    dr_putreg(c, oprnd.vc_reg, DR_I);
	    break;
#if SIZEOF_LONG != 4
	case SIZEOF_LONG:
	    dr_putreg(c, oprnd.vc_reg, DR_L);
	    break;
#else
	case 8:
	    /* simulate with double reg */
	    dr_putreg(c, oprnd.vc_reg, DR_I);
	    dr_putreg(c, oprnd.vc_reg2, DR_I);
	    REG_DEBUG(("put %d in free\n", _vrr(oprnd.vc_reg2)));
	    break;
#endif
	}
	break;
    case unsigned_type:
    case char_type:
	switch (oprnd.size) {
	case 1:		/* sizeof char */
	    dr_putreg(c, oprnd.vc_reg, DR_UC);
	    break;
	case 2:		/* sizeof short */
	    dr_putreg(c, oprnd.vc_reg, DR_US);
	    break;
	case 4:		/* sizeof int */
	    dr_putreg(c, oprnd.vc_reg, DR_U);
	    break;
#if SIZEOF_LONG != 4
	case SIZEOF_LONG:
	    dr_putreg(c, oprnd.vc_reg, DR_UL);
	    break;
#else
	case 8:
	    /* simulate with double reg */
	    dr_putreg(c, oprnd.vc_reg, DR_U);
	    dr_putreg(c, oprnd.vc_reg2, DR_U);
	    /* vc_reg2 holds high value */
	    REG_DEBUG(("put %d in free\n", _vrr(oprnd.vc_reg2)));
	    break;
#endif
	}
	break;
    case float_type:
	switch (oprnd.size) {
	case SIZEOF_FLOAT:	/* sizeof char */
	    dr_putreg(c, oprnd.vc_reg, DR_F);
	    break;
	case SIZEOF_DOUBLE:	/* sizeof short */
	    dr_putreg(c, oprnd.vc_reg, DR_D);
	    break;
	}
	break;
    case string_type:
	break;
    }
}

iogen_oprnd
gen_type_conversion(c, src_oprnd, data_type)
drisc_ctx c;
iogen_oprnd src_oprnd;
FMdata_type data_type;
{
    iogen_oprnd result_oprnd = src_oprnd;
    dr_reg_t at;  /* temporary */
    result_oprnd.data_type = data_type;
    switch (data_type) {
    case unknown_type:
    case char_type:
	assert(FALSE);
	break;
    case integer_type:
	result_oprnd.size = sizeof(long);
	if (!dr_getreg(c, &result_oprnd.vc_reg, DR_L, DR_TEMP))
	    gen_fatal("gen type convert out of registers \n");
	REG_DEBUG(("get %d in type_convert\n", _vrr(result_oprnd.vc_reg)));
	switch (src_oprnd.data_type) {
	case integer_type:
	case unknown_type:
	    assert(FALSE);
	    break;
	case boolean_type:
	case enumeration_type:
	case unsigned_type:
	case char_type:
	    switch (src_oprnd.size) {
	    case 1:
		dr_cvc2l(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 2:
		dr_cvus2l(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 4:
		dr_cvu2l(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 8:
#if SIZEOF_LONG == 8
		dr_cvul2l(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
#else
		result_oprnd.size = 8;
		if (!dr_getreg(c, &result_oprnd.vc_reg2, DR_L, DR_TEMP))
		    gen_fatal("gen type convert out of registers \n");
		REG_DEBUG(("get %d in type_convert\n", _vrr(result_oprnd.vc_reg2)));
		dr_cvul2l(c, result_oprnd.vc_reg2, src_oprnd.vc_reg2);
		dr_movul(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
#endif
		break;
	    }
	    break;
	case float_type:
	    switch (src_oprnd.size) {
	    case SIZEOF_FLOAT:
		dr_cvf2l(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case SIZEOF_DOUBLE:
		dr_cvd2l(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    default:
		assert(FALSE);
	    }
	    break;
	default:
	    assert(FALSE);
	    break;
	}
	break;
    case boolean_type:
    case enumeration_type:
    case unsigned_type:
	result_oprnd.size = sizeof(unsigned long);
	if (!dr_getreg(c, &result_oprnd.vc_reg, DR_UL, DR_TEMP))
	    gen_fatal("gen type convert out of registers \n");
	REG_DEBUG(("get %d in type_convert\n", _vrr(result_oprnd.vc_reg)));
	switch (src_oprnd.data_type) {
	case boolean_type:
	case enumeration_type:
	case unsigned_type:
	case unknown_type:
	    assert(FALSE);
	    break;
	case char_type:
	case integer_type:
	    switch (src_oprnd.size) {
	    case 1:
		dr_cvc2ul(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 2:
		dr_cvs2ul(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 4:
		dr_cvi2ul(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 8:
#if SIZEOF_LONG == 8
		dr_cvl2ul(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
#else
		result_oprnd.size = 8;
		if (!dr_getreg(c, &result_oprnd.vc_reg2, DR_UL, DR_TEMP))
		    gen_fatal("gen type convert out of registers \n");
		REG_DEBUG(("get %d in type_convert\n", _vrr(result_oprnd.vc_reg2)));
		dr_cvl2ul(c, result_oprnd.vc_reg2, src_oprnd.vc_reg2);
		dr_movul(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
#endif
		dr_cvul2l(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    }
	    break;
	case float_type:
	    switch (src_oprnd.size) {
	    case SIZEOF_FLOAT:
		dr_cvf2ul(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case SIZEOF_DOUBLE:
		dr_cvd2ul(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    default:
		assert(FALSE);
	    }
	    break;
	default:
	    assert(FALSE);
	}
	break;
    case float_type:
	result_oprnd.size = sizeof(double);
	if (!dr_getreg(c, &result_oprnd.vc_reg, DR_D, DR_TEMP))
	    gen_fatal("gen type convert out of registers \n");
	REG_DEBUG(("get %d in type_convert\n", _vrr(result_oprnd.vc_reg)));
	switch (src_oprnd.data_type) {
	case boolean_type:
	case enumeration_type:
	case unsigned_type:
	    switch (src_oprnd.size) {
	    case 1:
		if (!dr_getreg(c, &at, DR_L, DR_TEMP))
		    gen_fatal("gen type convert2 out of registers \n");
		dr_cvc2l(c, at, src_oprnd.vc_reg);
		dr_cvl2d(c, result_oprnd.vc_reg, at);
		dr_putreg(c, at, DR_L);
		break;
	    case 2:
		if (!dr_getreg(c, &at, DR_L, DR_TEMP))
		    gen_fatal("gen type convert2 out of registers \n");
		dr_cvs2l(c, at, src_oprnd.vc_reg);
		dr_cvl2d(c, result_oprnd.vc_reg, at);
		dr_putreg(c, at, DR_L);
		break;
	    case 4:
		if (!dr_getreg(c, &at, DR_L, DR_TEMP))
		    gen_fatal("gen type convert2 out of registers \n");
		dr_cvi2l(c, at, src_oprnd.vc_reg);
		dr_cvl2d(c, result_oprnd.vc_reg, at);
		dr_putreg(c, at, DR_L);
		break;
	    case 8:
#if SIZEOF_LONG == 8
		dr_cvl2d(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
#else
		{
		    dr_reg_t dat; /* temporary */
		    if (!dr_getreg(c, &dat, DR_D, DR_TEMP))
			gen_fatal("gen type convert2 out of registers \n");
		    dr_cvu2d(c, dat, src_oprnd.vc_reg);
		    dr_setd(c, result_oprnd.vc_reg, 4294967296.0);
		    dr_muld(c, result_oprnd.vc_reg, result_oprnd.vc_reg, dat);
		    dr_cvi2d(c, dat, src_oprnd.vc_reg);
		    dr_addd(c, result_oprnd.vc_reg, result_oprnd.vc_reg, dat);
		    dr_putreg(c, dat, DR_D);
		}
#endif
		break;
	    }
	    break;
	case char_type:
	case integer_type:
	    switch (src_oprnd.size) {
	    case 1:
		if (!dr_getreg(c, &at, DR_L, DR_TEMP))
		    gen_fatal("gen type convert2 out of registers \n");
		dr_cvc2l(c, at, src_oprnd.vc_reg);
		dr_cvl2d(c, result_oprnd.vc_reg, at);
		dr_putreg(c, at, DR_L);
		break;
	    case 2:
		if (!dr_getreg(c, &at, DR_L, DR_TEMP))
		    gen_fatal("gen type convert2 out of registers \n");
		dr_cvc2l(c, at, src_oprnd.vc_reg);
		dr_cvl2d(c, result_oprnd.vc_reg, at);
		dr_putreg(c, at, DR_L);
		break;
	    case 4:
		dr_cvi2d(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 8:
#if SIZEOF_LONG == 8
		dr_cvl2d(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
#else
		{
		    dr_reg_t dat; /* temporary */
		    if (!dr_getreg(c, &dat, DR_D, DR_TEMP))
			gen_fatal("gen type convert2 out of registers \n");
		    dr_cvu2d(c, dat, src_oprnd.vc_reg);
		    dr_setd(c, result_oprnd.vc_reg, 4294967296.0);
		    dr_muld(c, result_oprnd.vc_reg, result_oprnd.vc_reg, dat);
		    dr_cvi2d(c, dat, src_oprnd.vc_reg);
		    dr_addd(c, result_oprnd.vc_reg, result_oprnd.vc_reg, dat);
		    dr_putreg(c, dat, DR_D);
		}
#endif
		break;
	    }
	    break;
	case float_type:
	    assert(FALSE);
	    break;
	case unknown_type:
	default:
	    assert(FALSE);
	    break;
	}
	break;

    case string_type:
	assert(FALSE);
	break;
    }
    return result_oprnd;
}

iogen_oprnd
gen_size_conversion(c, src_oprnd, size)
drisc_ctx c;
iogen_oprnd src_oprnd;
int size;
{
    iogen_oprnd result_oprnd = src_oprnd;
    dr_reg_t at;  /* temporary */
    result_oprnd.size = size;
    switch (src_oprnd.data_type) {
    case integer_type:
	switch (size) {
	case 1:
	    if (!dr_getreg(c, &result_oprnd.vc_reg, DR_C, DR_TEMP))
		gen_fatal("gen size convert out of registers \n");
	    REG_DEBUG(("get %d in size convert\n", _vrr(result_oprnd.vc_reg)));
	    switch (src_oprnd.size) {
	    case 2:
		dr_cvs2l(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		dr_cvl2c(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 4:
		dr_cvi2c(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 8:
		dr_cvl2c(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    }
	    break;
	case 2:
	    if (!dr_getreg(c, &result_oprnd.vc_reg, DR_S, DR_TEMP))
		gen_fatal("gen size convert out of registers \n");
	    REG_DEBUG(("get %d in size convert\n", _vrr(result_oprnd.vc_reg)));
	    switch (src_oprnd.size) {
	    case 1:
		if (!dr_getreg(c, &at, DR_L, DR_TEMP))
		    gen_fatal("gen type convert2 out of registers \n");
		dr_cvc2l(c, at, src_oprnd.vc_reg);
		dr_cvl2s(c, result_oprnd.vc_reg, at);
		dr_putreg(c, at, DR_L);
		break;
	    case 4:
		dr_cvi2s(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 8:
		dr_cvl2s(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    }
	    break;
	case 4:
	    if (!dr_getreg(c, &result_oprnd.vc_reg, DR_I, DR_TEMP))
		gen_fatal("gen size convert out of registers E\n");
	    REG_DEBUG(("get %d in size convert\n", _vrr(result_oprnd.vc_reg)));
	    switch (src_oprnd.size) {
	    case 1:
		dr_cvc2i(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 2:
		dr_cvs2i(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 8:
		dr_cvl2i(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    }
	    break;
	case 8:
#if SIZEOF_LONG == 8
	    if (!dr_getreg(c, &result_oprnd.vc_reg, DR_L, DR_TEMP))
		gen_fatal("gen size convert out of registers \n");
	    REG_DEBUG(("get %d in size convert\n", _vrr(result_oprnd.vc_reg)));
	    switch (src_oprnd.size) {
	    case 1:
		dr_cvc2l(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 2:
		dr_cvs2l(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 4:
		dr_cvi2l(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    }
	    break;
#else
	    if (!dr_getreg(c, &result_oprnd.vc_reg, DR_L, DR_TEMP) ||
		(!dr_getreg(c, &result_oprnd.vc_reg2, DR_L, DR_TEMP)))
		gen_fatal("gen size convert out of registers \n");
	    REG_DEBUG(("get %d in size convert\n", _vrr(result_oprnd.vc_reg)));
	    REG_DEBUG(("get %d in size convert\n", _vrr(result_oprnd.vc_reg2)));
	    dr_setl(c, result_oprnd.vc_reg2, 0);
	    switch (src_oprnd.size) {
	    case 1:
		dr_cvc2l(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 2:
		dr_cvs2l(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 4:
		dr_cvi2l(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    }
	    break;
#endif
	}
	break;
    case boolean_type:
    case enumeration_type:
    case unsigned_type:
    case char_type:
	switch (size) {
	case 1:
	    if (!dr_getreg(c, &result_oprnd.vc_reg, DR_UC, DR_TEMP))
		gen_fatal("gen size convert out of registers \n");
	    REG_DEBUG(("get %d in size convert\n", _vrr(result_oprnd.vc_reg)));
	    switch (src_oprnd.size) {
	    case 2:
		dr_cvs2l(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		dr_cvl2c(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 4:
		dr_cvi2l(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		dr_cvl2c(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 8:
		dr_cvl2c(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    }
	    break;
	case 2:
	    if (!dr_getreg(c, &result_oprnd.vc_reg, DR_US, DR_TEMP))
		gen_fatal("gen size convert out of registers \n");
	    REG_DEBUG(("get %d in size convert\n", _vrr(result_oprnd.vc_reg)));
	    switch (src_oprnd.size) {
	    case 1:
		if (!dr_getreg(c, &at, DR_L, DR_TEMP))
		    gen_fatal("gen type convert2 out of registers \n");
		dr_cvc2l(c, at, src_oprnd.vc_reg);
		dr_cvl2s(c, result_oprnd.vc_reg, at);
		dr_putreg(c, at, DR_L);
		break;
	    case 4:
		dr_cvi2s(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 8:
		dr_cvl2s(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    }
	    break;
	case 4:
	    if (!dr_getreg(c, &result_oprnd.vc_reg, DR_U, DR_TEMP))
		gen_fatal("gen size convert out of registers \n");
	    REG_DEBUG(("get %d in size convert\n", _vrr(result_oprnd.vc_reg)));
	    switch (src_oprnd.size) {
	    case 1:
		dr_cvc2u(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 2:
		dr_cvs2u(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 8:
		dr_cvl2u(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    }
	    break;
	case 8:
#if SIZEOF_LONG == 8
	    if (!dr_getreg(c, &result_oprnd.vc_reg, DR_UL, DR_TEMP))
		gen_fatal("gen size convert out of registers \n");
	    REG_DEBUG(("get %d in size convert\n", _vrr(result_oprnd.vc_reg)));
	    switch (src_oprnd.size) {
	    case 1:
		dr_cvuc2ul(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 2:
		dr_cvus2ul(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 4:
		dr_cvu2ul(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    }
	    break;
#else
	    if (!dr_getreg(c, &result_oprnd.vc_reg, DR_UL, DR_TEMP) ||
		!dr_getreg(c, &result_oprnd.vc_reg2, DR_UL, DR_TEMP))
		gen_fatal("gen size convert out of registers \n");
	    REG_DEBUG(("get %d in size convert\n", _vrr(result_oprnd.vc_reg)));
	    REG_DEBUG(("get %d in size convert\n", _vrr(result_oprnd.vc_reg2)));
	    dr_setl(c, result_oprnd.vc_reg2, 0);
	    switch (src_oprnd.size) {
	    case 1:
		dr_cvc2ul(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 2:
		dr_cvs2ul(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    case 4:
		dr_cvi2ul(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    }
	    break;
#endif
	}
	break;
    case float_type:
	switch (size) {
	case SIZEOF_FLOAT:
	    if (!dr_getreg(c, &result_oprnd.vc_reg, DR_F, DR_TEMP))
		gen_fatal("gen size convert out of registers \n");
	    REG_DEBUG(("get %d in size convert\n", _vrr(result_oprnd.vc_reg)));
	    switch (src_oprnd.size) {
	    case SIZEOF_FLOAT:
		assert(FALSE);
		break;
	    case SIZEOF_DOUBLE:
		dr_cvd2f(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    default:
		assert(FALSE);
	    }
	    break;
	case SIZEOF_DOUBLE:
	    if (!dr_getreg(c, &result_oprnd.vc_reg, DR_D, DR_TEMP))
		gen_fatal("gen size convert out of registers \n");
	    REG_DEBUG(("get %d in size convert\n", _vrr(result_oprnd.vc_reg)));
	    REG_DEBUG(("get %d in size convert\n", _vrr(result_oprnd.vc_reg)));
	    switch (src_oprnd.size) {
	    case SIZEOF_DOUBLE:
		assert(FALSE);
		break;
	    case SIZEOF_FLOAT:
		dr_cvf2d(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    default:
		assert(FALSE);
	    }
	    break;
	default:
	    /* really should fail */
	    if (!dr_getreg(c, &result_oprnd.vc_reg, DR_D, DR_TEMP))
		gen_fatal("gen size convert out of registers \n");
	    REG_DEBUG(("get %d in size convert\n", _vrr(result_oprnd.vc_reg)));
	    REG_DEBUG(("get %d in size convert\n", _vrr(result_oprnd.vc_reg)));
	    switch (src_oprnd.size) {
	    case SIZEOF_DOUBLE:
/*		assert(FALSE);*/
		break;
	    case SIZEOF_FLOAT:
		dr_cvf2d(c, result_oprnd.vc_reg, src_oprnd.vc_reg);
		break;
	    default:
		assert(FALSE);
	    }
	    break;
	    
	}
	break;
    case unknown_type:
    default:
	assert(FALSE);
	break;
    }
    return result_oprnd;
}
