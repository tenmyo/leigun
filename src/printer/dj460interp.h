typedef struct Dj460Interp Dj460Interp;
int Dj460Interp_Feed(Dj460Interp * interp, void *buf, int len);
Dj460Interp *Dj460Interp_New(const char *name);
void Dj460Interp_Reset(Dj460Interp * interp);
