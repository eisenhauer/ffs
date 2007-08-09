typedef struct {
     int address;  /* if TRUE, reg contains the *address* of the data */
     FMdata_type data_type;
     int size;
     int offset;
     int aligned;
     int byte_swap;
     dr_reg_t vc_reg;
     dr_reg_t vc_reg2; /* used for paired regs to support non-native data types */
} iogen_oprnd, *iogen_oprnd_ptr;

iogen_oprnd
gen_set(drisc_ctx c, int size, char*value);

iogen_oprnd
gen_fetch(drisc_ctx c, dr_reg_t src_reg, int offset, int size,
		FMdata_type data_type, int aligned, int byte_swap);

iogen_oprnd
gen_operand(dr_reg_t src_reg, int offset, int size,
		  FMdata_type data_type, int aligned, int byte_swap);

void
gen_load(drisc_ctx c, iogen_oprnd_ptr src_oprnd);

void
gen_byte_swap(drisc_ctx c, iogen_oprnd_ptr src_oprnd);

void
gen_store(drisc_ctx c, 
		iogen_oprnd src, dr_reg_t dest_reg, int offset, int size,
		FMdata_type data_type, int aligned);

void
gen_memcpy(drisc_ctx c, dr_reg_t src, int src_offset, dr_reg_t dest,
		 int dest_offset, dr_reg_t size, int const_size);

void
free_oprnd(drisc_ctx c, iogen_oprnd oprnd);

iogen_oprnd
gen_type_conversion(drisc_ctx c, iogen_oprnd src_oprnd, FMdata_type data_type);

iogen_oprnd
gen_size_conversion(drisc_ctx c, iogen_oprnd src_oprnd, int size);

