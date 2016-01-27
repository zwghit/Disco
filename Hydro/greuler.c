
#include "../paul.h"
#include "metric.h"
#include "frame.h"

#define DEBUG 0

//Global Functions
double get_cs2( double );
double get_dp( double , double );
double get_dL( double * , double * , int );

//Local Functions
void cons2prim_prep(double *cons, double *x);
void cons2prim_solve_isothermal(double *cons, double *prim, double *x);
void cons2prim_solve_adiabatic(double *cons, double *prim, double *x);
void cons2prim_finalize(double *prim, double *x);

static double gamma_law = 0.0; 
static double RHO_FLOOR = 0.0; 
static double PRE_FLOOR = 0.0; 
static int isothermal = 0;

void setHydroParams( struct domain * theDomain ){
   gamma_law = theDomain->theParList.Adiabatic_Index;
   isothermal = theDomain->theParList.isothermal_flag;
   RHO_FLOOR = theDomain->theParList.Density_Floor;
   PRE_FLOOR = theDomain->theParList.Pressure_Floor;
}

int set_B_flag(void){
   return(0);
}

double get_omega(double *prim, double *x)
{
    int i,j;
    double l[3] = {prim[URR], prim[UPP], prim[UZZ]};
    
    double lapse;
    double shift[3];
    double igam[9];

    lapse = metric_lapse(x);
    metric_shift(x, shift);
    metric_igam(x, igam);

    double u2 = 0.0;
    for(i=0; i<3; i++)
        for(j=0; j<3; j++)
            u2 += igam[3*i+j]*l[i]*l[j];
    double w = sqrt(1.0 + u2);
    double vp = lapse*(igam[3]*l[0]+igam[4]*l[1]+igam[5]*l[2])/w - shift[1];
    
    return vp;
}

void prim2cons( double *prim, double *cons, double *x, double dV)
{
    double r = x[0];
    double rho = prim[RHO];
    double Pp  = prim[PPP];
    double l[3] = {prim[URR], prim[UPP], prim[UZZ]};

    double lapse;
    double shift[3];
    double igam[9];
    double jac;
    double U[4];

    lapse = metric_lapse(x);
    metric_shift(x, shift);
    metric_igam(x, igam);
    jac = metric_jacobian(x) / r;
    frame_U(x, U);
    double w, u0, u2;
    double u[3];
    double igaml[3];
    int i,j;
    for(i=0; i<3; i++)
    {
        igaml[i] = 0.0;
        for(j=0; j<3; j++)
            igaml[i] += igam[3*i+j]*l[j];
    }
    u2 = l[0]*igaml[0] + l[1]*igaml[1] + l[2]*igaml[2];
    w = sqrt(1.0 + u2);
    u0 = w/lapse;
    for(i=0; i<3; i++)
        u[i] = igaml[i] - shift[i]*u0;

    double l0 = -lapse*w + shift[0]*l[0] + shift[1]*l[1] + shift[2]*l[2];
    double uU = U[0]*l0 + U[1]*l[0] + U[2]*l[1] + U[3]*l[2];
    
    double rhoh = rho + gamma_law/(gamma_law-1.0)*Pp;
    double rhoe = Pp / (gamma_law-1.0);

    cons[DDD] = jac * rho*u0 * dV;
    cons[SRR] = jac * rhoh*u0*l[0] * dV;
    cons[LLL] = jac * rhoh*u0*l[1] * dV;
    cons[SZZ] = jac * rhoh*u0*l[2] * dV;
    cons[TAU] = jac * (-rhoe*uU*u0 - Pp*(uU*u0+U[0]) - rho*(uU+1)*u0) * dV;

    int q;
    for(q = NUM_C; q < NUM_Q; q++)
        cons[q] = prim[q]*cons[DDD];
}

void getUstar(double *prim, double *Ustar, double *x, double Sk, double Ss, 
                double *n, double *Bpack)
{
    Ustar[DDD] = 0.0;
    Ustar[SRR] = 0.0;
    Ustar[LLL] = 0.0;
    Ustar[SZZ] = 0.0;
    Ustar[TAU] = 0.0;

    int q;
    for(q = NUM_C; q < NUM_Q; q++)
        Ustar[q] = prim[q]*Ustar[DDD];
}

void cons2prim(double *cons, double *prim, double *x, double dV)
{
    int q;
    double cons1[NUM_Q];
    for(q=0; q<NUM_Q; q++)
        cons1[q] = cons[q]/dV;

    cons2prim_prep(cons1, x);
    if(isothermal)
        cons2prim_solve_isothermal(cons1, prim, x);
    else
        cons2prim_solve_adiabatic(cons1, prim, x);
    cons2prim_finalize(prim, x);
}

void flux(double *prim, double *flux, double *x, double *n)
{
    double r = x[0];
    double rho = prim[RHO];
    double Pp  = prim[PPP];
    double l[3] = {prim[URR], prim[UPP], prim[UZZ]};

    double lapse;
    double shift[3];
    double igam[9];
    double jac;
    double U[4];

    lapse = metric_lapse(x);
    metric_shift(x, shift);
    metric_igam(x, igam);
    jac = metric_jacobian(x) / r;
    frame_U(x, U);

    double w, u0, u2;
    double u[3];
    double igaml[3];
    int i,j;
    for(i=0; i<3; i++)
    {
        igaml[i] = 0.0;
        for(j=0; j<3; j++)
            igaml[i] += igam[3*i+j]*l[j];
    }
    u2 = l[0]*igaml[0] + l[1]*igaml[1] + l[2]*igaml[2];
    w = sqrt(1.0 + u2);
    u0 = w/lapse;
    for(i=0; i<3; i++)
        u[i] = igaml[i] - shift[i]*u0;

    double l0 = -lapse*w + shift[0]*l[0] + shift[1]*l[1] + shift[2]*l[2];
    double uU = U[0]*l0 + U[1]*l[0] + U[2]*l[1] + U[3]*l[2];
    //double un = u[0]*n[0] + r*u[1]*n[1] + u[2]*n[2];
    //double Un = U[1]*n[0] + r*U[2]*n[1] + U[3]*n[2];
    double un = u[0]*n[0] + u[1]*n[1] + u[2]*n[2];
    double Un = U[1]*n[0] + U[2]*n[1] + U[3]*n[2];
    double hn = n[0] + r*n[1] + n[2];
    
    double rhoh = rho + gamma_law/(gamma_law-1.0)*Pp;
    double rhoe = Pp / (gamma_law-1.0);

    //flux[DDD] = jac * rho*un;
    //flux[SRR] = jac * (rhoh*un*l[0] + Pp*n[0]);
    //flux[LLL] = jac * (rhoh*un*l[1] + Pp*n[1]*r);
    //flux[LLL] = jac * (rhoh*un*l[1] + Pp*n[1]);
    //flux[SZZ] = jac * (rhoh*un*l[2] + Pp*n[2]);
    //flux[TAU] = jac * (-rhoe*uU*un - Pp*(uU*un+Un) - rho*(uU+1)*un);

    flux[DDD] = jac * hn * rho*un;
    flux[SRR] = jac * hn * (rhoh*un*l[0] + Pp*n[0]);
    flux[LLL] = jac * hn * (rhoh*un*l[1] + Pp*n[1]);
    flux[SZZ] = jac * hn * (rhoh*un*l[2] + Pp*n[2]);
    flux[TAU] = jac * hn * (-rhoe*uU*un - Pp*(uU*un+Un) - rho*(uU+1)*un);
    int q;
    for(q = NUM_C; q < NUM_Q; q++)
        flux[q] = prim[q]*flux[DDD];
}

void source(double *prim, double *cons, double *xp, double *xm, double dVdt)
{
    double x[3] = {0.5*(xm[0]+xp[0]), 0.5*(xm[1]+xp[1]), 0.5*(xm[2]+xp[2])};
    double r = x[0];
    double rho = prim[RHO];
    double Pp  = prim[PPP];
    double l[4] = {0.0, prim[URR], prim[UPP], prim[UZZ]};

    int i,mu,nu;
    double lapse;
    double shift[3];
    double igam[9];
    double ig[16];
    double jac;
    double U[4], dU[16];

    lapse = metric_lapse(x);
    metric_shift(x, shift);
    metric_igam(x, igam);
    jac = metric_jacobian(x) / r;
    frame_U(x, U);
    frame_der_U(x, dU);

    double ia2 = 1.0/(lapse*lapse);
    ig[0] = -ia2;
    for(mu=0; mu<3; mu++)
    {
        ig[mu+1] = shift[mu]*ia2;
        ig[4*(mu+1)] = ig[mu+1];
        for(nu=0; nu<3; nu++)
            ig[4*(mu+1)+nu+1] = igam[3*mu+nu]-shift[mu]*shift[nu]*ia2;
    }

    double w, u[4], u2;
    double igaml[3];
    igaml[0] = igam[0]*l[1] + igam[1]*l[2] + igam[2]*l[3];
    igaml[1] = igam[3]*l[1] + igam[4]*l[2] + igam[5]*l[3];
    igaml[2] = igam[6]*l[1] + igam[7]*l[2] + igam[8]*l[3];
    u2 = l[1]*igaml[0] + l[2]*igaml[1] + l[3]*igaml[2];
    w = sqrt(1.0 + u2);
    
    u[0] = w/lapse;
    for(i=0; i<3; i++)
        u[i+1] = igaml[i] - shift[i]*u[0];
    l[0] = -lapse*w + shift[0]*l[1] + shift[1]*l[2] + shift[2]*l[3];
    
    double rhoh = rho + gamma_law/(gamma_law-1.0)*Pp;

    double S0, Sk[3];
    for(i=0; i<3; i++)
    {
        Sk[i] = 0.0;
        if(metric_killing(i+1))
            continue;

        double dg[16];
        metric_der_g(x, i+1, dg);
        for(mu=0; mu<4; mu++)
            for(nu=0; nu<4; nu++)
                Sk[i] += (rhoh*u[mu]*u[nu]+ig[4*mu+nu]*Pp)*dg[4*mu+nu];
        Sk[i] *= 0.5;
    }
    S0 = -U[1]*Sk[0] - U[2]*Sk[1] - U[3]*Sk[2];

    for(mu=1; mu<4; mu++)
        for(nu=0; nu<4; nu++)
        {
            if(mu == nu)
                S0 += -(rhoh*u[mu]*l[nu] + Pp) * dU[4*mu+nu];
            else
                S0 += -(rhoh*u[mu]*l[nu])*dU[4*mu+nu];
        }

    cons[SRR] += jac * Sk[0] * dVdt;
    cons[LLL] += jac * Sk[1] * dVdt;
    cons[SZZ] += jac * Sk[2] * dVdt;
    cons[TAU] += jac * S0 * dVdt;
}

void visc_flux(double *prim, double *gprim, double *flux, double *x, 
                double *n){}

void vel(double *prim1, double *prim2, double *Sl, double *Sr, double *Ss, 
            double *n, double *x, double *Bpack)
{
    double r = x[0];
    double rho1 = prim1[RHO];
    double P1   = prim1[PPP];
    double l1[3]  = {prim1[URR], prim1[UPP], prim1[UZZ]};

    double cs21 = gamma_law*P1/(rho1+gamma_law/(gamma_law-1.0)*P1);

    double rho2 = prim2[RHO];
    double P2   = prim2[PPP];
    double l2[3]  = {prim2[URR], prim2[UPP], prim2[UZZ]};

    double cs22 = gamma_law*P2/(rho2+gamma_law/(gamma_law-1.0)*P2);

    double a, b[3], gam[9], igam[9];
    a = metric_lapse(x);
    metric_shift(x, b);
    metric_gam(x, gam);
    metric_igam(x, igam);

    int i,j;
    double u21 = 0.0;
    double u22 = 0.0;
    double uS1[3], uS2[3];
    for(i=0; i<3; i++)
    {
        uS1[i] = 0.0;
        uS2[i] = 0.0;
        for(j=0; j<3; j++)
        {
            uS1[i] = igam[3*i+j]*l1[j];
            uS2[i] = igam[3*i+j]*l2[j];
        }
        u21 += uS1[i]*l1[i];
        u22 += uS2[i]*l2[i];
    }

    double w1 = sqrt(1.0+u21);
    double w2 = sqrt(1.0+u22);
    double v21 = u21/(w1*w1);
    double v22 = u22/(w2*w2);

    //TODO: Use n[] PROPERLY.  This only works if n = (1,0,0) or some 
    //      permutation.
    double vn1 = (uS1[0]*n[0]+uS1[1]*n[1]+uS1[2]*n[2]) / w1;
    double vn2 = (uS2[0]*n[0]+uS2[1]*n[1]+uS2[2]*n[2]) / w2;
    double bn = (b[0]*n[0]+b[1]*n[1]+b[2]*n[2]);
    double ign = igam[3*0+0]*n[0] + igam[3*1+1]*n[1] + igam[3*2+2]*n[2];

    double dv1 = sqrt(cs21*(ign - vn1*vn1 - cs21*(ign*v21-vn1*vn1))) / w1;
    double dv2 = sqrt(cs22*(ign - vn2*vn2 - cs22*(ign*v22-vn2*vn2))) / w2;
    double hn = n[0] + r*n[1] + n[2];

    double sl1 = hn * (a * (vn1*(1.0-cs21) - dv1) / (1.0-v21*cs21) - bn);
    double sr1 = hn * (a * (vn1*(1.0-cs21) + dv1) / (1.0-v21*cs21) - bn);
    double sl2 = hn * (a * (vn2*(1.0-cs22) - dv2) / (1.0-v22*cs22) - bn);
    double sr2 = hn * (a * (vn2*(1.0-cs22) + dv2) / (1.0-v22*cs22) - bn);

    *Sr = sr1 > sr2 ? sr1 : sr2;
    *Sl = sl1 < sl2 ? sl1 : sl2;

    //TODO: USE REAL HLLC SPEED THIS IS WRONG
    *Ss = 0.5*(*Sl + *Sr);

}

double mindt(double *prim, double wc, double *xp, double *xm)
{
    double x[3] = {0.5*(xm[0]+xp[0]), 0.5*(xm[1]+xp[1]), 0.5*(xm[2]+xp[2])};
    double r = x[0];
    double rho = prim[RHO];
    double Pp  = prim[PPP];
    double l[3] = {prim[URR], prim[UPP], prim[UZZ]};
    double cs  = sqrt(gamma_law*Pp/(rho+gamma_law/(gamma_law-1)*Pp));

    double a, b[3], gam[9], igam[9];
    a = metric_lapse(x);
    metric_shift(x, b);
    metric_gam(x, gam);
    metric_igam(x, igam);

    int i,j;
    double uS[3], u2;
    for(i=0; i<3; i++)
    {
        uS[i] = 0.0;
        for(j=0; j<3; j++)
            uS[i] += igam[3*i+j]*l[j];
        u2 += uS[i]*l[i];
    }
    double w = sqrt(1.0+u2);

    double v2, vS[3];
    for(i=0; i<3; i++)
        vS[i] = uS[i]/w;
    v2 = u2/(w*w);

    double sig = 1-cs*cs;

    double dvr = cs * sqrt(igam[0]*(1-cs*cs*v2) - sig*vS[0]*vS[0]) / w;
    double vrl = fabs(a * (vS[0]*sig - dvr) / (1-v2*cs*cs) - b[0]);
    double vrr = fabs(a * (vS[0]*sig + dvr) / (1-v2*cs*cs) - b[0]);

    double dvp = cs * sqrt(igam[4]*(1-cs*cs*v2) - sig*vS[1]*vS[1]) / w;
    double vpl = fabs(r * (a * (vS[1]*sig - dvr) / (1-v2*cs*cs) - b[1]));
    double vpr = fabs(r * (a * (vS[1]*sig + dvr) / (1-v2*cs*cs) - b[1]));
    
    double dvz = cs * sqrt(igam[8]*(1-cs*cs*v2) - sig*vS[2]*vS[2]) / w;
    double vzl = fabs(a * (vS[2]*sig - dvr) / (1-v2*cs*cs) - b[2]);
    double vzr = fabs(a * (vS[2]*sig + dvr) / (1-v2*cs*cs) - b[2]);

    double maxvr = vrr > vrl ? vrr : vrl;
    double maxvp = vpr > vpl ? vpr : vpl;
    double maxvz = vzr > vzl ? vzr : vzl;

    double dtr = get_dL(xp,xm,1)/maxvr;
    double dtp = get_dL(xp,xm,0)/maxvp;
    double dtz = get_dL(xp,xm,2)/maxvz;

    double dt = dtr;
    dt = dt < dtp ? dt : dtp;
    dt = dt < dtz ? dt : dtz;

    return dt;
}

double getReynolds(double *prim, double w, double *x, double dx)
{
    return 0.0;
}

void cons2prim_prep(double *cons, double *x)
{
    //TODO: complete this.
}

void cons2prim_solve_isothermal(double *cons, double *prim, double *x)
{
    //TODO: complete this.
    int q;
    for( q=NUM_C ; q<NUM_Q ; ++q )
        prim[q] = cons[q]/cons[DDD];
}

void cons2prim_solve_adiabatic(double *cons, double *prim, double *x)
{
    double prec = 1.0e-14;
    double max_iter = 100;

    double r = x[0];

    double D = cons[DDD];
    double S[3] = {cons[SRR], cons[LLL], cons[SZZ]};
    double tau = cons[TAU];

    double lapse;
    double shift[3];
    double igam[9];
    double jac;
    double U[4];
    lapse = metric_lapse(x);
    metric_shift(x, shift);
    metric_igam(x, igam);
    jac = metric_jacobian(x) / r;
    frame_U(x, U);

    double s2 = 0.0;
    double Us = 0.0;

    int i,j;
    for(i=0; i<3; i++)
        for(j=0; j<3; j++)
            s2 += igam[3*i+j]*S[i]*S[j];
    s2 /= D*D;

    for(i=0; i<3; i++)
        Us += S[i]*(shift[i]*U[0] + U[i+1]);
    Us /= D;
    
    double e = (tau/D + Us + 1.0) / (lapse*U[0]);
    double n = (gamma_law-1.0)/gamma_law;

    if(e*e < s2 && DEBUG)
    {
        printf("Not enough thermal energy (r=%.12lg, e2=%.12lg, s2=%.12lg)\n",
                r, e*e, s2);

        double cons0[NUM_Q];
        prim2cons(prim, cons0, x, 1.0);

        printf("prim: %.16lg %.16lg %.16lg %.16lg %.16lg\n",
                prim[RHO], prim[PPP], prim[URR], prim[UPP], prim[UZZ]);
        printf("cons0: %.16lg %.16lg %.16lg %.16lg %.16lg\n",
                cons0[DDD], cons0[TAU], cons0[SRR], cons0[LLL], cons0[SZZ]);
        printf("cons: %.16lg %.16lg %.16lg %.16lg %.16lg\n",
                cons[DDD], cons[TAU], cons[SRR], cons[LLL], cons[SZZ]);
    }

    double wmo;
    if(s2 == 0.0)
        wmo = 0.0;
    else
    {
        //Newton-Raphson on a quartic polynomial to solve for w-1
        //TODO: Minimize truncation error.
        double c[5];
        c[0] = -s2*(n-1)*(n-1);
        c[1] = 2*((e-n)*(e-n)+2*(n-1)*s2);
        c[2] = (5*e-n)*(e-n)-2*(3-n)*s2;
        c[3] = 4*(e*e-s2) - 2*n*e;
        c[4] = e*e-s2;

        //Bounds
        double wmomin = 0.0; //u=0
        double wmomax = sqrt(1.0+s2)-1.0; //eps = P = 0

        //Initial guess: previous w
        double u2 = 0.0;
        double l[3] = {prim[URR], prim[UPP], prim[UZZ]};
        for(i=0; i<3; i++)
            for(j=0; j<3; j++)
                u2 += igam[3*i+j]*l[i]*l[j];
        double wmo0 = u2 / (1.0+sqrt(1.0+u2)); // sqrt(1+u2)-1

        //Run Newton-Raphson
        double wmo1 = wmo0;
        i = 0;
        do
        {
            //TODO: Telescoping evaluation.
            wmo = wmo1;
            double f = c[0] + c[1]*wmo + c[2]*wmo*wmo + c[3]*wmo*wmo*wmo
                        + c[4]*wmo*wmo*wmo*wmo;
            double df = c[1] + 2*c[2]*wmo + 3*c[3]*wmo*wmo
                        + 4*c[4]*wmo*wmo*wmo;
            wmo1 = wmo - f/df;

            if(f > 0.0 && wmo<wmomax)
                wmomax = wmo;
            else if(f < 0.0 && wmo > wmomin)
                wmomin = wmo;
            if(wmo1 < wmomin || wmo1 > wmomax)
                wmo1 = 0.5*(wmomin+wmomax);
            i++;
        }
        while(fabs((wmo-wmo1)/(wmo+1.0)) > prec && i < max_iter);

        if(i == max_iter && DEBUG)
            printf("ERROR: NR failed to converge\n");
    }

    //Prim recovery
    double w = wmo + 1.0;
    double u0 = w/lapse;
    double hmo = w*(e-w) / (w*w-n);

    double rho = D / (jac*u0);
    if(rho < RHO_FLOOR)
        rho = RHO_FLOOR;
    double Pp = n * rho * hmo;
    if(Pp < PRE_FLOOR*rho)
        Pp = PRE_FLOOR*rho;
    
    double h = 1.0 + gamma_law/(gamma_law-1.0) * Pp/rho;
    double l[3] = {S[0]/(D*h), S[1]/(D*h), S[2]/(D*h)};

    prim[RHO] = rho;
    prim[URR] = l[0];
    prim[UPP] = l[1];
    prim[UZZ] = l[2];
    prim[PPP] = Pp;

    int q;
    for( q=NUM_C ; q<NUM_Q ; ++q )
        prim[q] = cons[q]/cons[DDD];
    
    if(e*e < s2 && DEBUG)
    {
        double cons1[NUM_Q];
        prim2cons(prim, cons1, x, 1.0);

        printf("prim1: %.16lg %.16lg %.16lg %.16lg %.16lg\n",
                prim[RHO], prim[PPP], prim[URR], prim[UPP], prim[UZZ]);
        printf("cons1: %.16lg %.16lg %.16lg %.16lg %.16lg\n",
                cons1[DDD], cons1[TAU], cons1[SRR], cons1[LLL], cons1[SZZ]);
    }
}

void cons2prim_finalize(double *prim, double *x)
{

}
