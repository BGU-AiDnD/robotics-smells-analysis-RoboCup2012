/*#ifndef XFUZZY_INFERENCE_ENGINE
#define XFUZZY_INFERENCE_ENGINE

/***************************************/
/*  Type TP_areaJogador */
/**************************************

static TP_areaJogador createTP_areaJogador() {
 TP_areaJogador tp;
 double min = 0.0;
 double max = 105.0;
 double step = 0.4117647058823529;
 double _p_mtPerto[4] = { -9.545454545454545,0.0,14.49090909090909,21.636363636363633 };
 double _p_perto[4] = { 19.09090909090909,21.636363636363633,35.72727272727273,43.272727272727266 };
 double _p_medio[4] = { 39.72727272727273,44.272727272727266,66.36363636363636,76.9090909090909 };
 double _p_longe[4] = { 73.36363636363636,76.9090909090909,105.0,114.54545454545453 };
 tp.mtPerto = createMF_xfl_trapezoid(min,max,step,_p_mtPerto,4);
 tp.perto = createMF_xfl_trapezoid(min,max,step,_p_perto,4);
 tp.medio = createMF_xfl_trapezoid(min,max,step,_p_medio,4);
 tp.longe = createMF_xfl_trapezoid(min,max,step,_p_longe,4);
 return tp;
}

/***************************************/
/*  Type TP_posXgoleiro */
/**************************************

static TP_posXgoleiro createTP_posXgoleiro() {
 TP_posXgoleiro tp;
 double min = 0.0;
 double max = 45.0;
 double step = 0.17647058823529413;
 double _p_recuado[4] = { -4.090909090909091,0.0,3.481818181818182,5.472727272727273 };
 double _p_avancadoArea[4] = { 4.581818181818182,5.672727272727273,9.454545454545453,15.545454545454547 };
 double _p_medio[4] = { 13.454545454545453,15.545454545454547,23.72727272727273,29.81818181818182 };
 double _p_avancado[4] = { 27.72727272727273,29.81818181818182,45.0,49.09090909090909 };
 tp.recuado = createMF_xfl_trapezoid(min,max,step,_p_recuado,4);
 tp.avancadoArea = createMF_xfl_trapezoid(min,max,step,_p_avancadoArea,4);
 tp.medio = createMF_xfl_trapezoid(min,max,step,_p_medio,4);
 tp.avancado = createMF_xfl_trapezoid(min,max,step,_p_avancado,4);
 return tp;
}

/***************************************/
/*  Type TP_possChute */
/**************************************

static TP_possChute createTP_possChute() {
 TP_possChute tp;
 double min = 0.0;
 double max = 10.0;
 double step = 0.0392156862745098;
 double _p_baixa[4] = { -1.25,0.0,2.5,3.25 };
 double _p_media[4] = { 2.9,3.75,6.25,7.0 };
 double _p_alta[4] = { 6.7,7.5,10.0,11.25 };
 tp.baixa = createMF_xfl_trapezoid(min,max,step,_p_baixa,4);
 tp.media = createMF_xfl_trapezoid(min,max,step,_p_media,4);
 tp.alta = createMF_xfl_trapezoid(min,max,step,_p_alta,4);
 return tp;
}

/***************************************/
/*  Type TP_areaHori */
/**************************************

static TP_areaHori createTP_areaHori() {
 TP_areaHori tp;
 double min = 0.0;
 double max = 68.0;
 double step = 0.26666666666666666;
 double _p_direita[4] = { -8.5,0.0,17.0,25.5 };
 double _p_meio[4] = { 17.0,25.5,42.5,51.0 };
 double _p_esquerda[4] = { 42.5,51.0,68.0,76.5 };
 tp.direita = createMF_xfl_trapezoid(min,max,step,_p_direita,4);
 tp.meio = createMF_xfl_trapezoid(min,max,step,_p_meio,4);
 tp.esquerda = createMF_xfl_trapezoid(min,max,step,_p_esquerda,4);
 return tp;
}

/***************************************/
/*  Type TP_qtJogadores */
/**************************************

static TP_qtJogadores createTP_qtJogadores() {
 TP_qtJogadores tp;
 double min = 0.0;
 double max = 10.0;
 double step = 0.0392156862745098;
 double _p_vantagem[4] = { -0.9090909090909091,0.0,2.5,3.5 };
 double _p_empate[4] = { 3.0,3.5,5.5,6.0 };
 double _p_desvantagem[4] = { 5.5,6.2,7.5,7.9 };
 double _p_largaDes[4] = { 7.5,8.181818181818182,10.0,10.909090909090908 };
 tp.vantagem = createMF_xfl_trapezoid(min,max,step,_p_vantagem,4);
 tp.empate = createMF_xfl_trapezoid(min,max,step,_p_empate,4);
 tp.desvantagem = createMF_xfl_trapezoid(min,max,step,_p_desvantagem,4);
 tp.largaDes = createMF_xfl_trapezoid(min,max,step,_p_largaDes,4);
 return tp;
}

/***************************************/
/*  MembershipFunction MF_xfl_trapezoid  */
/**************************************
static double MF_xfl_trapezoid_equal(MembershipFunction _mf,double x) {
 double a = _mf.param[0];
 double b = _mf.param[1];
 double c = _mf.param[2];
 double d = _mf.param[3];
  return (x<a || x>d? 0: (x<b? (x-a)/(b-a) : (x<c?1 : (d-x)/(d-c))));
}
static double MF_xfl_trapezoid_greq(MembershipFunction _mf,double x) {
 double a = _mf.param[0];
 double b = _mf.param[1];
 double c = _mf.param[2];
 double d = _mf.param[3];
  return (x<a? 0 : (x>b? 1 : (x-a)/(b-a) ));
}
static double MF_xfl_trapezoid_smeq(MembershipFunction _mf,double x) {
 double a = _mf.param[0];
 double b = _mf.param[1];
 double c = _mf.param[2];
 double d = _mf.param[3];
  return (x<c? 1 : (x>d? 0 : (d-x)/(d-c) ));
}
static double MF_xfl_trapezoid_center(MembershipFunction _mf) {
 double a = _mf.param[0];
 double b = _mf.param[1];
 double c = _mf.param[2];
 double d = _mf.param[3];
  return (b+c)/2;
}
static double MF_xfl_trapezoid_basis(MembershipFunction _mf) {
 double a = _mf.param[0];
 double b = _mf.param[1];
 double c = _mf.param[2];
 double d = _mf.param[3];
  return (d-a);
}
static MembershipFunction createMF_xfl_trapezoid( double min, double max, double step, double *param,int length) {
 int i;
 MembershipFunction _mf;
 _mf.min = min;
 _mf.max = max;
 _mf.step = step;
 _mf.param = (double*) malloc(length*sizeof(double));
 for(i=0;i<length;i++) _mf.param[i] = param[i];
 _mf.compute_eq = MF_xfl_trapezoid_equal;
 _mf.compute_greq = MF_xfl_trapezoid_greq;
 _mf.compute_smeq = MF_xfl_trapezoid_smeq;
 _mf.center = MF_xfl_trapezoid_center;
 _mf.basis = MF_xfl_trapezoid_basis;
 return _mf;
}

typedef struct {
 double (* And)();
 double (* Or)();
 double (* also)();
 double (* imp)();
 double (* Not)();
 double (* very)();
 double (* moreorless)();
 double (* slightly)();
 double (* defuz)();
} OperatorSet;

typedef struct {
 double min;
 double max;
 double step;
 double *param;
 double (* compute_eq)();
 double (* compute_greq)();
 double (* compute_smeq)();
 double (* center)();
 double (* basis)();
} MembershipFunction;

typedef struct {
 int crisp;
 double crispvalue;
 int length;
 double *degree;
 MembershipFunction *conc;
 int inputlength;
 double *input;
 OperatorSet op;
} FuzzyNumber;


#endif /* XFUZZY_INFERENCE_ENGINE 

#ifndef XFUZZY_ultimaTentativa1
#define XFUZZY_ultimaTentativa1


typedef struct {
 MembershipFunction mtPerto;
 MembershipFunction perto;
 MembershipFunction medio;
 MembershipFunction longe;
} TP_areaJogador;

typedef struct {
 MembershipFunction recuado;
 MembershipFunction avancadoArea;
 MembershipFunction medio;
 MembershipFunction avancado;
} TP_posXgoleiro;

typedef struct {
 MembershipFunction baixa;
 MembershipFunction media;
 MembershipFunction alta;
} TP_possChute;

typedef struct {
 MembershipFunction direita;
 MembershipFunction meio;
 MembershipFunction esquerda;
} TP_areaHori;

typedef struct {
 MembershipFunction vantagem;
 MembershipFunction empate;
 MembershipFunction desvantagem;
 MembershipFunction largaDes;
} TP_qtJogadores;
 void RL_definePoss(FuzzyNumber areaCampo, FuzzyNumber numJogadores, FuzzyNumber *poss);
 void ultimaTentativa1InferenceEngine(double _d_possibilidades, double _d_areaCampo,
                                      double _d_areaAtual, double _d_numJogadores,
                                      double *_d_respostaGoleiro, double *_d_poss);

#endif /* XFUZZY_ultimaTentativa1 */
