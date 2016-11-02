/*
 ***************************************************************************************
 * Debug-Exporting of variables
 ***************************************************************************************
 */

int DbgExport(int type,void *dataP,const char *format,...) __attribute__ ((format (printf, 3, 4)));
void DbgVars_Init(void);



/*
 ***********************************************
 * Make the compiler warning if type is wrong
 ***********************************************
 */
#define CHECK_TYPE(type,var) {		\
        typedef type TestType(void);	\
        TestType *testProc;		\
        typeof(var) LaberBla(void) {return (typeof(var))0;};	\
        testProc = LaberBla;		\
}
/*
 *****************************************************************
 * Dbg_Export(Bla->temperature,"%s.temperature",instancename);
 * All variables have host byteorder because they are generated 
 * on the host
 *****************************************************************
 */

#define DBGT_UINT8_T	(1)
#define DBGT_UINT16_T	(2)
#define DBGT_UINT32_T	(3)
#define DBGT_UINT64_T	(4)
#define DBGT_INT8_T	(5)
#define DBGT_INT16_T	(6)
#define DBGT_INT32_T	(7)
#define DBGT_INT64_T	(8)
#define DBGT_DOUBLE_T	(9)
#define DBGT_PROC64_T	(19)

#define DbgExport_U8(dt,x...) \
        CHECK_TYPE(uint8_t, dt); \
        DbgExport(DBGT_UINT8_T,&dt,x);

#define DbgExport_U16(dt,x...) \
        CHECK_TYPE(uint16_t, dt); \
        DbgExport(DBGT_UINT16_T,&dt,x);

#define DbgExport_U32(dt,x...) \
        CHECK_TYPE(uint32_t, dt); \
        DbgExport(DBGT_UINT32_T,&dt,x);

#define DbgExport_U64(dt,x...) \
        CHECK_TYPE(uint64_t, dt); \
        DbgExport(DBGT_UINT64_T,&dt,x);

#define DbgExport_S8(dt,x...) \
        CHECK_TYPE(uint8_t, dt); \
        DbgExport(DBGT_INT8_T,&dt,x);

#define DbgExport_S16(dt,x...) \
        CHECK_TYPE(uint16_t, dt); \
        DbgExport(DBGT_INT16_T,&dt,x);

#define DbgExport_S32(dt,x...) \
        CHECK_TYPE(uint32_t, dt); \
        DbgExport(DBGT_INT32_T,&dt,x);

#define DbgExport_S64(dt,x...) \
        CHECK_TYPE(uint64_t, dt); \
        DbgExport(DBGT_INT64_T,&dt,x);

#define DbgExport_DBL(dt,x...) \
        CHECK_TYPE(double, dt); \
        DbgExport(DBGT_DOUBLE_T,&dt,x);

typedef void DbgSetSymProc(void *clientData,uint32_t arg,uint64_t value);
typedef uint64_t DbgGetSymProc(void *clientData,uint32_t arg);

int DbgSymHandler(DbgSetSymProc*,DbgGetSymProc,void *cd,uint32_t arg,const char *format,...) \
	 __attribute__ ((format (printf, 5, 6)));
