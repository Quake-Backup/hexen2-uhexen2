// view.h

extern	cvar_t		v_gamma;

extern	byte		gammatable[256];	// palette is sent through this
extern	unsigned short	ramps[3][256];
extern float v_blend[4];

void V_Init (void);
void V_RenderView (void);
float V_CalcRoll (vec3_t angles, vec3_t velocity);
void V_CalcBlend (void);
void V_UpdatePalette (void);

