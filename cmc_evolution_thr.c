/* -*- linux-c -*- */

/* use commandline option -DUSE_THREADS to turn on threading */
/*#define USE_THREADS*/
#ifdef USE_THREADS
#include <pthread.h>
#define NUM_THREADS	2
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include "cmc.h"
#include "cmc_vars.h"
 
#ifdef USE_CUDA
#include "cuda/cmc_cuda.h"
#endif

/* function to calculate r_p, r_a, dQ/dr|_r_p, and dQ/dr|_r_a for an orbit */
orbit_rs_t calc_orbit_rs(long si, double E, double J)
{
	orbit_rs_t orbit_rs;
	long ktemp=si, kmin, kmax, i, i1, fevals;
	double Qtemp, rk, rk1, Uk, Uk1, a, b, rmin, dQdr_min, rmax, dQdr_max;
	double dQdr_min_num, dQdr_max_num, inside_sqrt, actual_rmin, actual_rmax;

	/* for newly created stars, position si is not ordered */
        //dprintf("clus.N_MAX= %li\n", clus.N_MAX);
	if (si > clus.N_MAX) {
		// Not sure how this stupid linear search got here in the first place...
		//ktemp = 0;
		//while (ktemp < clus.N_MAX && star[ktemp].r < star[si].rnew) {
		//	ktemp++;
		//}
		// Replaced by much more efficient bisection...
		ktemp = FindZero_r(0, clus.N_MAX, star[si].rnew) + 1;
	}

	/* Q(si) is positive for a standard object that has undergone relaxation 
	   (see Joshi, Rasio, & Portegies Zwart 2000).  However, it can be negative
	   for newly created stars.  We ignore this for now and hope for the best. */
	Qtemp = function_Q(si, ktemp, E, J);

	fevals=1; 
	/* check for nearly circular orbit and do linear search */
	if (Qtemp < 0.0) {
		ktemp = -1;
		do {
			ktemp++; fevals++;
			Qtemp = function_Q(si, ktemp, E, J);
		} while (Qtemp < 0.0 && ktemp <= clus.N_MAX);		
		if (ktemp >= clus.N_MAX) {
			dprintf("linear search failed\n");
			ktemp = si;
		}
	}
	//printf("==> index=%li, fevals=%li\n", si, fevals);

	if (Qtemp <= 0) { /* possibly a circular orbit */
		while (function_Q(si, ktemp + 1, E, J) > function_Q(si, ktemp, E, J) 
		       && function_Q(si, ktemp, E, J) < 0 && ktemp <= clus.N_MAX) {
			ktemp++;
		}
		while (function_Q(si, ktemp - 1, E, J) > function_Q(si, ktemp, E, J) 
		       && function_Q(si, ktemp, E, J) < 0 && ktemp >= 1) {
			ktemp--;
		}
		
		/* check to see if star is on almost circular orbit */
		if (function_Q(si, ktemp, E, J) < 0) {
			dprintf("circular orbit found: si=%ld sr=%g svr=%g svt=%g J=%g E=%g\n",
				si, star[si].r, star[si].vr, star[si].vt, star[si].J, star[si].E);
		}
		
#ifdef USE_MPI
		orbit_rs.rp = star_r[si];
		orbit_rs.ra = star_r[si];
#else
		orbit_rs.rp = star[si].r;
		orbit_rs.ra = star[si].r;
#endif
		orbit_rs.kmin = si;
		orbit_rs.kmax = si;
		orbit_rs.dQdrp = 0.0;
		orbit_rs.dQdra = 0.0;
		orbit_rs.circular_flag = 1;
	} else {
#ifdef USE_CUDA
		kmin = h_kmin[si];
		kmax = h_kmax[si];
#else
		kmin = FindZero_Q(si, 0, ktemp, E, J);
		kmax = FindZero_Q(si, ktemp, clus.N_MAX + 1, E, J);
#endif
		
		while (function_Q(si, kmin, E, J) > 0 && kmin > 0)
			kmin--;
		while (function_Q(si, kmin + 1, E, J) < 0 
		       && kmin + 1 < ktemp)
			kmin++;
	
		i = kmin;
		i1 = kmin + 1;
#ifdef USE_MPI
		rk = star_r[i];
		rk1 = star_r[i1];
		Uk = star_phi[i] + MPI_PHI_S(rk, si);
		Uk1 = star_phi[i1] + MPI_PHI_S(rk1, si);
#else
		rk = star[i].r;
		rk1 = star[i1].r;
		Uk = star[i].phi + PHI_S(rk, si);
		Uk1 = star[i1].phi + PHI_S(rk1, si);
#endif
		
		a = (Uk1 - Uk) / (1 / rk1 - 1 / rk);
		b = (Uk / rk1 - Uk1 / rk) / (1 / rk1 - 1 / rk);
		
		/*Sourav: Avoiding a negative argument for the sqrt in rmin */
		inside_sqrt = a * a - 2.0 * J * J * (b - E);
		if (inside_sqrt<0.0){
			rmin = -1.0 *J * J / a ;
			actual_rmin = J * J / (-a + sqrt(a * a - 2.0 * J * J * (b - E)));
			dprintf("The sqrt in the expression for rmin has a negative argument!");
			dprintf("It is, therefore, set to zero.\n");
			dprintf("rmin_old= %g rmin_new= %g rmin_sqrt= %g\n", actual_rmin, rmin, inside_sqrt);
			dprintf("star[kmin+1].r= %g star[kmin].r= %g star[kmin+1].r-star[kmin].r= %g\n",
				star[kmin+1].r,star[kmin].r,star[kmin+1].r-star[kmin].r);
			dprintf("star[kmin+1].r-rmin= %g, rmin-star[kmin].r= %g\n",star[kmin+1].r-rmin,rmin-star[kmin].r);

		}
		else{
			// /*rmin = J * J / (-a + sqrt(a * a - 2.0 * J * J * (b - E)));*/
			rmin = J * J / (-a + sqrt(inside_sqrt));
		}
		
		dQdr_min = 2.0 * J * J / (rmin * rmin * rmin) + 2.0 * a / (rmin * rmin);
#ifdef USE_MPI
		dQdr_min_num = (function_Q(si, kmin+1, E, J)-function_Q(si, kmin, E, J))/(star_r[kmin+1]-star_r[kmin]);
#else
		dQdr_min_num = (function_Q(si, kmin+1, E, J)-function_Q(si, kmin, E, J))/(star[kmin+1].r-star[kmin].r);
#endif              
 
		/*  For rmax- Look for rk, rk1 such that 
		 *  Q(rk) > 0 > Q(rk1) */
		
		while (function_Q(si, kmax, E, J) < 0 && kmax > ktemp)
			kmax--;
		while (kmax + 1 < clus.N_MAX
		       && function_Q(si, kmax + 1, E, J) > 0 )
			kmax++;
		
		i = kmax;
		i1 = kmax + 1;

#ifdef USE_MPI
		rk = star_r[i];
		rk1 = star_r[i1];
		Uk = star_phi[i] + MPI_PHI_S(rk, si);
		Uk1 = star_phi[i1] + MPI_PHI_S(rk1, si);
#else		
		rk = star[i].r;
		rk1 = star[i1].r;
		Uk = star[i].phi + PHI_S(rk, si);
		Uk1 = star[i1].phi  + PHI_S(rk1, si);
#endif

		a = (Uk1 - Uk) / (1. / rk1 - 1. / rk);
		b = (Uk / rk1 - Uk1 / rk) / (1. / rk1 - 1. / rk);
	        
		/*Sourav: Avoiding a negative argument for the sqrt in rmax */
		inside_sqrt = a * a - 2.0 * J * J * (b - E);
		if (inside_sqrt<0.0){
			rmax = -a / (2.0 * (b - E));
			actual_rmax = (-a + sqrt(a * a - 2.0 * J * J * (b - E))) / (2.0 * (b - E));
			dprintf("The sqrt in the expression for rmax has a negative argument!");
			dprintf("It is, therefore, set to zero.\n");
			dprintf("rmax_old= %g rmax_new= %g rmax_sqrt= %g\n", actual_rmax, rmax, inside_sqrt);
			dprintf("star[kmin+1].r= %g star[kmin].r= %g star[kmin+1].r-star[kmin].r= %g\n",
				star[kmin+1].r,star[kmin].r,star[kmin+1].r-star[kmin].r);
			dprintf("star[kmin+1].r-rmin= %g, rmin-star[kmin].r= %g\n",star[kmin+1].r-rmin,rmin-star[kmin].r);
		}
		else{
		/*rmax = (-a + sqrt(a * a - 2.0 * J * J * (b - E))) / (2.0 * (b - E));*/
		  rmax = (-a + sqrt(inside_sqrt)) / (2.0 * (b - E));
		}

		dQdr_max = 2.0 * J * J / (rmax * rmax * rmax) + 2.0 * a / (rmax * rmax);
#ifdef USE_MPI
		dQdr_max_num = (function_Q(si, kmax+1, E, J)-function_Q(si, kmax, E, J))/(star_r[kmax+1]-star_r[kmax]);
#else
		dQdr_max_num = (function_Q(si, kmax+1, E, J)-function_Q(si, kmax, E, J))/(star[kmax+1].r-star[kmax].r);
#endif

		/* another case of a circular orbit */
		if ((rmin > rmax)||(dQdr_min<=0.)||(dQdr_max>=0.)) {
		        eprintf("circular orbit found!\n");
		 	eprintf("Check Here: rmin=%g>rmax=%g: kmin=%ld kmax=%ld si=%ld r=%g vr=%g vt=%g J=%g E=%g Q(kmin)=%g Q(kmax)=%g dQdr_min=%g dQdr_min=%g dQdr_min_num=%g dQdr_max_num=%g\n",
				rmin, rmax, kmin, kmax, si, star[si].r, star[si].vr, star[si].vt, star[si].J, star[si].E,
				function_Q(si, kmin, star[si].E, star[si].J), function_Q(si, kmax, star[si].E, star[si].J),
				dQdr_min,dQdr_max,dQdr_min_num,dQdr_max_num);

#ifdef USE_MPI
			orbit_rs.rp = star_r[si];
			orbit_rs.ra = star_r[si];
#else
			orbit_rs.rp = star[si].r;
			orbit_rs.ra = star[si].r;
#endif
			orbit_rs.kmin = si;
			orbit_rs.kmax = si;
			orbit_rs.dQdrp = 0.0;
			orbit_rs.dQdra = 0.0;
			orbit_rs.circular_flag = 1;
		} else {
			orbit_rs.rp = rmin;
			orbit_rs.ra = rmax;
			orbit_rs.kmin= kmin;
			orbit_rs.kmax= kmax;
			orbit_rs.dQdrp = dQdr_min;
			orbit_rs.dQdra = dQdr_max;
			orbit_rs.circular_flag = 0;	
			orbit_rs.kmax= kmax;
			orbit_rs.kmin= kmin;
		}

#ifdef EXPERIMENTAL
		/* Consistency check for rmin and rmax. If it fails, we bisect our way through.*/
		if (!orbit_rs.circular_flag) {
			int rmax_in_interval=0, rmin_in_interval=0, vr_rmax_positive=0, vr_rmin_positive=0;

			rmin_in_interval= orbit_rs.rp< star[kmin+1].r && orbit_rs.rp> star[kmin].r;
			rmax_in_interval= orbit_rs.ra< star[kmax+1].r && orbit_rs.ra> star[kmax].r;
			if (rmin_in_interval)
				vr_rmin_positive= calc_vr_in_interval(orbit_rs.rp, si, kmin, E, J)>= 0.;
			if (rmax_in_interval)
				vr_rmax_positive= calc_vr_in_interval(orbit_rs.ra, si, kmax, E, J)>= 0.;

			if (!(rmax_in_interval && vr_rmax_positive)) {
				rmax= find_root_vr(si, kmax, E, J);
				i = kmax;
				i1 = kmax + 1;
				rk = star[i].r;
				rk1 = star[i1].r;
				Uk = star[i].phi + PHI_S(rk, si);
				Uk1 = star[i1].phi  + PHI_S(rk1, si);		
				a = (Uk1 - Uk) / (1. / rk1 - 1. / rk);
				dQdr_max = 2.0 * J * J / (rmax * rmax * rmax) + 2.0 * a / (rmax * rmax);
				orbit_rs.ra= rmax;
				orbit_rs.dQdra = dQdr_max;
			};

			if (!(rmin_in_interval && vr_rmin_positive)) {
				rmin= find_root_vr(si, kmin, E, J);
				i = kmin;
				i1 = kmin + 1;
				rk = star[i].r;
				rk1 = star[i1].r;
				Uk = star[i].phi + PHI_S(rk, si);
				Uk1 = star[i1].phi + PHI_S(rk1, si);
				a = (Uk1 - Uk) / (1 / rk1 - 1 / rk);
				dQdr_min = 2.0 * J * J / (rmin * rmin * rmin) + 2.0 * a / (rmin * rmin);
				orbit_rs.rp= rmin;
				orbit_rs.dQdrp = dQdr_min;
			};
		};
#endif
	}
	
	return(orbit_rs);
}

double GetTimeStep(gsl_rng *rng) {
	double DTrel, Tcoll, DTcoll, Tbb, DTbb, Tbs, DTbs, Tse, DTse, Trejuv, DTrejuv, xcoll;

	/* calculate the relaxation timestep */
	if (RELAXATION || FORCE_RLX_STEP) {
		//Optimize simul_relax() later 
		//DTrel = simul_relax(rng);

//MPI2: Testing: MPI and Serial versions dont match exactly because of the way the averaging and binning are done in the MPI version. However, I think it is ok since this is only an estimate of the timestep. So, doesnt have to be exactly same. Later might need to change the mpi version to exactly simulate simul_relax_new();.
#ifdef USE_MPI
		DTrel = mpi_simul_relax_new();
#else
		//Optimized version of simul_relax()
		DTrel = simul_relax_new();
#endif

// Testing
//This is for testing the timestep variation over a number of timesteps, and see if the old and the new versions are at least nearly same.
//if(myid==0){
/*
		static int a = 0;
		a++;
		FILE *fTest;
		fTest = fopen("simul_relax_test.dat", "a");
		fprintf(fTest, "%d\t%g\t%g\n",a ,DTrel, temp);
		fclose(fTest);
*/

	} else {
		DTrel = GSL_POSINF;
	}
	Dt = DTrel;

#ifdef USE_MPI
if(myid==0)
#endif
{
	/* calculate DTcoll, using the expression from Freitag & Benz (2002) (their paper II) */
	if (central.N_sin != 0 && SS_COLLISION) {
		/* X defines pericenter needed for collision: r_p = X (R_1+R_2) */
		if (TIDAL_CAPTURE) {
			xcoll = XCOLLTC;
		} else {
			xcoll = XCOLLSS;
		}
		Tcoll = 1.0 / (16.0 * sqrt(PI) * central.n_sin * sqr(xcoll) * (central.v_sin_rms/sqrt(3.0)) * central.R2_ave * 
			       (1.0 + central.mR_ave/(2.0*xcoll*sqr(central.v_sin_rms/sqrt(3.0))*central.R2_ave))) * 
			log(GAMMA * ((double) clus.N_STAR)) / ((double) clus.N_STAR);
		fprintf (stdout, "Time = %f Gyr Tcoll = %f Gyr\n", 
			TotalTime*clus.N_STAR*units.t/log(GAMMA*clus.N_STAR)/YEAR/1e+09,
			Tcoll*clus.N_STAR*units.t/log(GAMMA*clus.N_STAR)/YEAR/1e+09);
	} else {
		Tcoll = GSL_POSINF;
	}
	DTcoll = 5.0e-3 * Tcoll;
	Dt = MIN(Dt, DTcoll);

	/* calculate DTbb, using a generalization of the expression for Tcoll */
	if (central.N_bin != 0 && BINBIN) {
		/* X defines pericenter needed for "strong" interaction: r_p = X (a_1+a_2) */	
		Tbb = 1.0 / (16.0 * sqrt(PI) * central.n_bin * sqr(XBB) * (central.v_bin_rms/sqrt(3.0)) * central.a2_ave * 
			     (1.0 + central.ma_ave/(2.0*XBB*sqr(central.v_bin_rms/sqrt(3.0))*central.a2_ave))) * 
			log(GAMMA * ((double) clus.N_STAR)) / ((double) clus.N_STAR);
	} else {
		Tbb = GSL_POSINF;
	}
	DTbb = 5.0e-3 * Tbb;
	Dt = MIN(Dt, DTbb);

	/* calculate DTbs, using a generalization of the expression for Tcoll */
	if (central.N_bin != 0 && central.N_sin != 0 && BINSINGLE) {
		/* X defines pericenter needed for "strong" interaction: r_p = X a */
		Tbs = 1.0 / (4.0 * sqrt(PI) * central.n_sin * sqr(XBS) * (central.v_rms/sqrt(3.0)) * central.a2_ave * 
			     (1.0 + central.m_ave*central.a_ave/(XBS*sqr(central.v_rms/sqrt(3.0))*central.a2_ave))) * 
			log(GAMMA * ((double) clus.N_STAR)) / ((double) clus.N_STAR);
	} else {
		Tbs = GSL_POSINF;
	}
	DTbs = 5.0e-3 * Tbs;
	Dt = MIN(Dt, DTbs);

	/* calculate DTse, for now using the SE mass loss from the previous step as an indicator
	   for this step; in the future perhaps we can get an estimate of the mass loss rate
	   from SSE/BSE */
	if (!STELLAR_EVOLUTION || DMse == 0.0 || tcount == 1) {
		DTse = GSL_POSINF;
	} else {
		/* DMse can be negative due to round-off error.  Give up if |DMse| is statistically
		   significantly larger than the round-off error. */
		if (DMse < -1.0e-10 * ((double) clus.N_STAR)) {
			eprintf("DMse = %g < -1.0e-10 * ((double) clus.N_STAR)!\n", DMse);
			exit_cleanly(-1);
		}
		/* get timescale for 1% mass loss from cluster */
		Tse = 0.01 * Mtotal / (fabs(DMse) / Prev_Dt);
		/* and take a fraction of that for the timestep */
		DTse = 0.1 * Tse;
	}
	Dt = MIN(Dt, DTse);

	//Sourav: Toy rejuvenation prescription, early mass loss indication for timestep
	if (!STAR_AGING_SCHEME || DMrejuv == 0.0){
		DTrejuv = GSL_POSINF;
	} else {
		if (DMrejuv<0.0) {
			eprintf("DMrejuv = %g < 0.0!\n", DMrejuv);
			exit_cleanly(-1);
		}
		/* get timescale for 0.1% mass loss from cluster */
		Trejuv = 0.01 * Mtotal / (fabs(DMrejuv) / Prev_Dt);
		DTrejuv = 0.01 * Trejuv; //Check if this fraction can make virial ratio better
		printf ("THIS IS WHERE THE TIMESCALE GOT SET: T= %f DT=%f DM=%f\n", Trejuv, DTrejuv, DMrejuv);
		printf ("*****************************\n"); //checking what's going on
	}	
	Dt = MIN(Dt, DTrejuv);
	
	/* take a reasonable timestep if all physics is turned off */
	if (Dt == GSL_POSINF) {
		Dt = 0.001;
	}

	/* debugging */
	dprintf("Dt=%g DTrel=%g DTcoll=%g DTbb=%g DTbs=%g DTse=%g DTrejuv=%g\n", Dt, DTrel, DTcoll, DTbb, DTbs, DTse, DTrejuv);

}
	return (Dt);
}

/* removes tidally-stripped stars */
void tidally_strip_stars(void) {
	double phi_rtidal, phi_zero, gierszalpha;
	long i, j, k;
	k=0;
	
	j = 0;
	Etidal = 0.0;
	OldTidalMassLoss = TidalMassLoss;
	/******************************/
	/* get new particle positions */
	/******************************/
	max_r = get_positions();
	DTidalMassLoss = TidalMassLoss - OldTidalMassLoss;
		
	gprintf("tidally_strip_stars(): iteration %ld: OldTidalMassLoss=%.6g DTidalMassLoss=%.6g\n",
		j, OldTidalMassLoss, DTidalMassLoss);
	fprintf(logfile, "tidally_strip_stars(): iteration %ld: OldTidalMassLoss=%.6g DTidalMassLoss=%.6g\n",
		j, OldTidalMassLoss, DTidalMassLoss);

	
	/* Iterate the removal of tidally stripped stars 
	 * by reducing Rtidal */
	do {
		Rtidal = orbit_r * pow(Mtotal 
			- (TidalMassLoss - OldTidalMassLoss), 1.0/3.0);
		phi_rtidal = potential(Rtidal);
		phi_zero = potential(0.0);
		DTidalMassLoss = 0.0;

//MPI2: There's a problem in this loop since Etidal needs to be checked everytime. Might need to be done sequentially. Oh wait!, cant be done sequentially yoo, since other variables for other stars after N/procs are not present on root node! So, moving this just before sorting tidally_strip_stars2() and doing it on root.

		/* XXX maybe we should use clus.N_MAX_NEW below?? */
		for (i = 1; i <= clus.N_MAX; i++) 
		{
			if (TIDAL_TREATMENT == 0){
				/*radial cut off criteria*/

				if (star[i].r_apo > Rtidal && star[i].rnew < 1000000) { 
					dprintf("tidally stripping star with r_apo > Rtidal: i=%ld id=%ld m=%g E=%g binind=%ld\n", i, star[i].id, star[i].m, star[i].E, star[i].binind);
					star[i].rnew = SF_INFINITY;	/* tidally stripped star */
					star[i].vrnew = 0.0;
					star[i].vtnew = 0.0;
					Eescaped += star[i].E * star[i].m / clus.N_STAR;
					Jescaped += star[i].J * star[i].m / clus.N_STAR;
					if (star[i].binind == 0) {
						Eintescaped += star[i].Eint;
					} else {
						Ebescaped += -(binary[star[i].binind].m1/clus.N_STAR) * (binary[star[i].binind].m2/clus.N_STAR) / 
							(2.0 * binary[star[i].binind].a);
						Eintescaped += binary[star[i].binind].Eint1 + binary[star[i].binind].Eint2;
					}

					DTidalMassLoss += star[i].m / clus.N_STAR;
					Etidal += star[i].E * star[i].m / clus.N_STAR;

					/* logging */
					fprintf(escfile,
							"%ld %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %ld ",
							tcount, TotalTime, star[i].m,
							star[i].r, star[i].vr, star[i].vt, star[i].r_peri,
							star[i].r_apo, Rtidal, phi_rtidal, phi_zero, star[i].E, star[i].J, star[i].id);

					if (star[i].binind) {
						k = star[i].binind;
						fprintf(escfile, "1 %.8g %.8g %ld %ld %.8g %.8g ", 
								binary[k].m1 * (units.m / clus.N_STAR) / MSUN, 
								binary[k].m2 * (units.m / clus.N_STAR) / MSUN, 
								binary[k].id1, binary[k].id2,
								binary[k].a * units.l / AU, binary[k].e);
					} else {
						fprintf(escfile, "0 0 0 0 0 0 0 ");	
					}

					if (star[i].binind == 0) {
						fprintf(escfile, "%d na na ", 
								star[i].se_k);
					} else {
						fprintf(escfile, "na %d %d",
								binary[k].bse_kw[0], binary[k].bse_kw[1]);
					}
					fprintf (escfile, "\n");



					/* perhaps this will fix the problem wherein stars are ejected (and counted)
					   multiple times */
					dprintf ("before SE: id=%ld k=%ld kw=%d m=%g mt=%g R=%g L=%g mc=%g rc=%g menv=%g renv=%g ospin=%g epoch=%g tms=%g tphys=%g phi=%g r=%g\n",
		     star[i].id,i,star[i].se_k,star[i].se_mass,star[i].se_mt,star[i].se_radius,star[i].se_lum,star[i].se_mc,star[i].se_rc,
	     		star[i].se_menv,star[i].se_renv,star[i].se_ospin,star[i].se_epoch,star[i].se_tms,star[i].se_tphys,star[i].phi, star[i].r);
					destroy_obj(i);
					if (Etotal.K + Etotal.P - Etidal >= 0)
						break;

				}
			}


			else if (TIDAL_TREATMENT == 1){
				/* DEBUG: Now using Giersz prescription for tidal stripping 
				   (Giersz, Heggie, & Hurley 2008; arXiv:0801.3709).
				   Note that this alpha factor behaves strangely for small N (N<~10^3) */

				gierszalpha = 1.5 - 3.0 * pow(log(GAMMA * ((double) clus.N_STAR)) / ((double) clus.N_STAR), 0.25);
				if (star[i].E > gierszalpha * phi_rtidal && star[i].rnew < 1000000) {
					dprintf("tidally stripping star with E > phi rtidal: i=%ld id=%ld m=%g E=%g binind=%ld\n", i, star[i].id, star[i].m, star[i].E, star[i].binind); 
					star[i].rnew = SF_INFINITY;	/* tidally stripped star */
					star[i].vrnew = 0.0;
					star[i].vtnew = 0.0;
					Eescaped += star[i].E * star[i].m / clus.N_STAR;
					Jescaped += star[i].J * star[i].m / clus.N_STAR;
					if (star[i].binind == 0) {
						Eintescaped += star[i].Eint;
					} else {
						Ebescaped += -(binary[star[i].binind].m1/clus.N_STAR) * (binary[star[i].binind].m2/clus.N_STAR) / 
							(2.0 * binary[star[i].binind].a);
						Eintescaped += binary[star[i].binind].Eint1 + binary[star[i].binind].Eint2;
					}

					DTidalMassLoss += star[i].m / clus.N_STAR;
					Etidal += star[i].E * star[i].m / clus.N_STAR;

					/* logging */
					fprintf(escfile,
							"%ld %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %.8g %ld ",
							tcount, TotalTime, star[i].m,
							star[i].r, star[i].vr, star[i].vt, star[i].r_peri,
							star[i].r_apo, Rtidal, phi_rtidal, phi_zero, star[i].E, star[i].J, star[i].id);

					if (star[i].binind) {
						k = star[i].binind;
						fprintf(escfile, "1 %.8g %.8g %ld %ld %.8g %.8g ", 
								binary[k].m1 * (units.m / clus.N_STAR) / MSUN, 
								binary[k].m2 * (units.m / clus.N_STAR) / MSUN, 
								binary[k].id1, binary[k].id2,
								binary[k].a * units.l / AU, binary[k].e);
					} else {
						fprintf(escfile, "0 0 0 0 0 0 0 ");	
					}

					if (star[i].binind == 0) {
						fprintf(escfile, "%d na na ", 
								star[i].se_k);
					} else {
						fprintf(escfile, "na %d %d",
								binary[k].bse_kw[0], binary[k].bse_kw[1]);
					}
					fprintf (escfile, "\n");



					/* perhaps this will fix the problem wherein stars are ejected (and counted)
					   multiple times */
					destroy_obj(i);

					if (Etotal.K + Etotal.P - Etidal >= 0)
						break;

				}
			}

		}

		j++;
		TidalMassLoss = TidalMassLoss + DTidalMassLoss;
		gprintf("tidally_strip_stars(): iteration %ld: TidalMassLoss=%.6g DTidalMassLoss=%.6g\n",
				j, TidalMassLoss, DTidalMassLoss);
		fprintf(logfile, "tidally_strip_stars(): iteration %ld: TidalMassLoss=%.6g DTidalMassLoss=%.6g\n",
				j, TidalMassLoss, DTidalMassLoss);
	} while (DTidalMassLoss > 0 && (Etotal.K + Etotal.P - Etidal) < 0);
}

void remove_star(long j, double phi_rtidal, double phi_zero) {
	double E, J;
	long k=0;
	//double temp=0;

	/* dprintf("removing star: i=%ld id=%ld m=%g E=%g bin=%ld\n", j, star[j].id, star[j].m, star[j].E, star[j].binind); */

	E = star[j].E;
	J = star[j].J;
	star[j].rnew = SF_INFINITY;	/* tidally stripped star */
	star[j].vrnew = 0.0;
	star[j].vtnew = 0.0;

#ifdef USE_MPI
	Eescaped += E * star_m[j] / clus.N_STAR;
	Jescaped += J * star_m[j] / clus.N_STAR;
#else
	Eescaped += E * star[j].m / clus.N_STAR;
	Jescaped += J * star[j].m / clus.N_STAR;
#endif

	if (star[j].binind == 0) {
		Eintescaped += star[j].Eint;
	} else {
		Ebescaped += -(binary[star[j].binind].m1/clus.N_STAR) * (binary[star[j].binind].m2/clus.N_STAR) / 
			(2.0 * binary[star[j].binind].a);
		Eintescaped += binary[star[j].binind].Eint1 + binary[star[j].binind].Eint2;
	}

#ifdef USE_MPI
	TidalMassLoss += star_m[j] / clus.N_STAR;
	Etidal += E * star_m[j] / clus.N_STAR;
#else
	TidalMassLoss += star[j].m / clus.N_STAR;
	Etidal += E * star[j].m / clus.N_STAR;
#endif

	/* logging */
	fprintf(escfile, "%ld %.8g %.8g ",
		tcount, TotalTime, star[j].m);
	fprintf(escfile, "%.8g %.8g %.8g ",
		star[j].r, star[j].vr, star[j].vt);
	fprintf(escfile, "%.8g %.8g %.8g %.8g %.8g %.8g %.8g %ld ",
	        star[j].r_peri, star[j].r_apo, Rtidal, phi_rtidal, phi_zero, E, J, star[j].id);
	if (star[j].binind) {
		k = star[j].binind;
		fprintf(escfile, "1 %.8g %.8g %ld %ld %.8g %.8g ", 
				binary[k].m1 * (units.m / clus.N_STAR) / MSUN, 
				binary[k].m2 * (units.m / clus.N_STAR) / MSUN, 
				binary[k].id1, binary[k].id2,
				binary[k].a * units.l / AU, binary[k].e);
	} else {
		fprintf(escfile, "0 0 0 0 0 0 0 ");	
	}

	if (star[j].binind == 0) {
		fprintf(escfile, "%d na na ", 
				star[j].se_k);
	} else {
		fprintf(escfile, "na %d %d ",
				binary[k].bse_kw[0], binary[k].bse_kw[1]);
	}
	fprintf (escfile, "\n");

	/* perhaps this will fix the problem wherein stars are ejected (and counted)
	   multiple times */
	destroy_obj(j);
/*
#ifdef USE_MPI
	MPI_Reduce(&Eescaped, &temp, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);		
	if(myid==0)
		Eescaped = temp;
	MPI_Bcast(&Eescaped, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	MPI_Reduce(&Jescaped, &temp, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);		
	if(myid==0)
		Jescaped = temp;
	MPI_Bcast(&Jescaped, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	MPI_Reduce(&Eintescaped, &temp, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);		
	if(myid==0)
		Eintescaped = temp;
	MPI_Bcast(&Eintescaped, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	MPI_Reduce(&Ebescaped, &temp, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);		
	if(myid==0)
		Ebescaped = temp;
	MPI_Bcast(&Ebescaped, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	MPI_Reduce(&TidalMassLoss, &temp, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);		
	if(myid==0)
		TidalMassLoss R temp;
	MPI_Bcast(&TidalMassLoss, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	MPI_Reduce(&Etidal, &temp, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);		
	if(myid==0)
		Etidal = temp;
	MPI_Bcast(&Etidal, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif
*/
}

void remove_star_center(long j) {
	star[j].rnew = SF_INFINITY;	/* send star to infinity         */
#ifdef USE_MPI
	star_m[j] = DBL_MIN;		/* set mass to very small number */
#else
	star[j].m = DBL_MIN;		/* set mass to very small number */
#endif
	star[j].vrnew = 0.0;		/* setup vr and vt for           */
	star[j].vtnew = 0.0;		/*		future calculations  */
}

/**************** Get Positions and Velocities ***********************/
/*	Requires indexed (sorted in increasing r) stars with potential
	computed in star[].phi and N_MAX set. Uses sE[], sJ[] and sr[]
	from previous iteration. Returns positions and velocities in
	srnew[], svrnew[], and svtnew[]. Returns Max r for all stars, or
	-1 on error. */
void get_positions_loop(struct get_pos_str *get_pos_dat){
	long j, k, si;
	double r=0.0, vr=0.0, vt, rmin, rmax, E, J;
	double g1, g2, F, s0, g0, dQdr_min, dQdr_max, drds, max_rad, pot;
	double X=0.0, Q;	/* Max value of Q found. Should be > 0 */
	long N_LIMIT;
	double phi_rtidal, phi_zero;
	orbit_rs_t orbit_rs;

	N_LIMIT = get_pos_dat->N_LIMIT;
	phi_rtidal = get_pos_dat->phi_rtidal;
	phi_zero = get_pos_dat->phi_zero;
	max_rad = get_pos_dat->max_rad;

#ifdef USE_CUDA
	cuCalculateKs();
#endif

#ifdef USE_MPI 
	//MPI2: Only running till N_MAX for now since no new stars are created, later new loop has to be introduced from N_MAX+1 to N_MAX_NEW.
	for (si=mpiBegin; si<=mpiEnd; si++) {
#else
	//MPI2: Running till N_MAX since findProcForIndex will cause a problem since currently start creation is not handled properly.
	for (si = 1; si <= clus.N_MAX_NEW; si++) { /* Repeat for all stars */
	//for (si = 1; si <= clus.N_MAX; si++) { /* Repeat for all stars */

#endif
		j = si;

#ifdef USE_MPI
		E = star[j].E + MPI_PHI_S(star_r[j], j);
#else
		E = star[j].E + PHI_S(star[j].r, j);
#endif
		J = star[j].J;

/*
		if(isnan(star[j].E))
			printf("**************si = %ld\t id = %ld*****************\n", si, star[si].id);
*/		
		/* remove massless stars (boundary stars or stellar evolution victims) */
		/* note that energy lost due to stellar evolution is subtracted
		   at the time of mass loss in DoStellarEvolution */
#ifdef USE_MPI
		if (star_m[j] < ZERO) {
#else
		if (star[j].m < ZERO) {
#endif
			destroy_obj(j);
			//printf("index of stripped star = %ld\n", j);
			if(j==99865)
				printf("hi m=%g\t se_k=%d\t id=%ld\n", star[j].m, star[j].se_k, star[j].id);
			continue;
		}

		/* remove unbound stars */
		if (E >= 0.0) {
		/*	dprintf("tidally stripping star with E >= 0: i=%ld id=%ld m=%g E=%g binind=%ld\n", j, star[j].id, star[j].m, star[j].E, star[j].binind); */
			remove_star(j, phi_rtidal, phi_zero);
			//printf("index of stripped star = %ld\tE = %g\n",j,star[j].E);
			if(j==99865)
				printf("hi E\n");
			continue;
		}

		/* calculate peri- and apocenter of orbit */
#ifdef EXPERIMENTAL
		orbit_rs = calc_orbit_new(j, E, J);
#else
		orbit_rs = calc_orbit_rs(j, E, J);
#endif		

		/* skip the rest if the star is on a nearly circular orbit */
		if (orbit_rs.circular_flag == 1) {
#ifdef USE_MPI
			star[si].rnew = star_r[si];
#else
			star[si].rnew = star[si].r;
#endif
			star[si].vrnew = star[si].vr;
			star[si].vtnew = star[si].vt;
			continue;
		} else {
			rmin = orbit_rs.rp;
			rmax = orbit_rs.ra;
			dQdr_min = orbit_rs.dQdrp;
			dQdr_max = orbit_rs.dQdra;
		}

		/* Check for rmax > R_MAX (tidal radius) */
		if (rmax >= Rtidal) {
			/* dprintf("tidally stripping star with rmax >= Rtidal: i=%ld id=%ld m=%g E=%g binind=%ld\n", j, star[j].id, star[j].m, star[j].E, star[j].binind); */
			star[j].r_apo= rmax;
			star[j].r_peri= rmin;
			remove_star(j, phi_rtidal, phi_zero);
			continue;
		}

		g1 = sqrt(3.0 * (rmax - rmin) / dQdr_min);	/* g(-1) */
		g2 = sqrt(-3.0 * (rmax - rmin) / dQdr_max);	/* g(+1) */

		F = 1.2 * MAX(g1, g2);


#ifndef USE_MPI
		curr_st = &st[findProcForIndex(j)];
		if(j > clus.N_MAX)
			printf("id = %ld\tDrawing rand. num from node %d \tN_MAX=%ld\n", j, findProcForIndex(j), clus.N_MAX);
#endif

		for (k = 1; k <= N_TRY; k++) {
			X = rng_t113_dbl_new(curr_st);
			//X = rng_t113_dbl();

			s0 = 2.0 * X - 1.0;	 /* random -1 < s0 < 1 */

			g0 = F * rng_t113_dbl_new(curr_st);
			//g0 = F * rng_t113_dbl(); /* random  0 < g0 < F */

			r = 0.5 * (rmin + rmax) + 0.25 * (rmax - rmin) * (3.0 * s0 - s0 * s0 * s0);

#ifdef USE_MPI
			pot = potential(r) + MPI_PHI_S(r, j);
#else
			pot = potential(r) + PHI_S(r, j);
#endif

			drds = 0.25 * (rmax - rmin) * (3.0 - 3.0 * s0 * s0);
			Q = 2.0 * E - 2.0 * pot - J * J / r / r;

			if (Q >= 0.0) {
				vr = sqrt(Q);
			} else {
				dprintf("circular orbit: vr^2<0: setting vr=0: si=%ld r=%g rmin=%g rmax=%g vr^2=%g X=%g E=%g J=%g\n", si, r, rmin, rmax, Q, X, E, J);
				if (isnan(Q)) {
					//printf("E=%g\tpot=%g\tJ=%g\tr=%g\trmin=%g\trmax=%g\tkmin=%d\tkmax=%d\tstar_r[25000]=%g\tstar_m[25000]=%g\tstar_phi[25000]=%g\t\n", E, pot, J, r, rmin, rmax, orbit_rs.kmin, orbit_rs.kmax,star_r[25000],star_m[25000],star_phi[25000]);
					eprintf("si = %ld \tfatal error: Q=vr^2==nan!\n",si);
					exit_cleanly(-1);
				}
				vr = 0;
			}
			if (g0 < 1.0 / vr * drds)	/* if g0 < g(s0) then success! */
				break;
		}

		if (k == N_TRY + 1) {
			eprintf("N_TRY exceeded\n");
			exit_cleanly(-1);
		}

		/* remove stars if they are too close to center,
		 * ie. r < MINIMUM_R.
		 * Add their mass to CentralMass */
		//if (rmax < MINIMUM_R){
		/* (r<MINIMUM_R && rmin>0.3*rmax){ */
		MINIMUM_R = 2.0 * FB_CONST_G * cenma.m * units.mstar / fb_sqr(FB_CONST_C) / units.l;
		if (0) {
		/* if (r < MINIMUM_R) { */
#ifdef USE_MPI
			cenma.m += star_m[j];
			cenma.E += (2.0*star_phi[j] + star[j].vr * star[j].vr + star[j].vt * star[j].vt) / 
				2.0 * star_m[j] * madhoc;
			//Reduction?? For now it is ok, since if(0) never runs :)
#else
			cenma.m += star[j].m;
			cenma.E += (2.0*star[j].phi + star[j].vr * star[j].vr + star[j].vt * star[j].vt) / 
				2.0 * star[j].m * madhoc;
#endif
			destroy_obj(j);
			MINIMUM_R = 2.0 * FB_CONST_G * cenma.m * units.mstar / fb_sqr(FB_CONST_C) / units.l;
			continue;
		}

		star[j].X = X;
		star[j].r_peri = rmin;
		star[j].r_apo = rmax;
		
		/* pick random sign for v_r */
		//if(rng_t113_dbl() < 0.5)
		if(rng_t113_dbl_new(curr_st) < 0.5)
			vr = -vr;

		vt = J / r;

		star[j].rnew = r;
		star[j].vrnew = vr;
		star[j].vtnew = vt;

		if (r > max_rad)
			max_rad = r;
	} /* Next si */
	get_pos_dat->max_rad = max_rad;
}

/* return maximum stellar radius, get r_p and r_a for all objects */
double get_positions(){
#ifdef USE_THREADS
	pthread_t threads[NUM_THREADS];
	pthread_attr_t attr;
	int rc, t;
	struct get_pos_str *get_positions_data_array;
#endif
	double max_rad, phi_rtidal, phi_zero;
	long N_LIMIT;
	struct get_pos_str get_positions_data;

#ifdef USE_THREADS
	/* Initialize and set thread detached attribute */
	pthread_attr_init(&attr);
	get_positions_data_array = malloc(NUM_THREADS*sizeof(struct get_pos_str));
#endif

	max_rad = 0.0;	/* max radius for all stars, returned on success */

	phi_rtidal = potential(Rtidal);
	phi_zero = potential(0.0);

	N_LIMIT = clus.N_MAX;

	get_positions_data.max_rad = max_rad;
	get_positions_data.phi_rtidal = phi_rtidal;
	get_positions_data.phi_zero = phi_zero;
	get_positions_data.N_LIMIT = N_LIMIT;
	get_positions_data.taskid = 0;
	get_positions_data.CMincr.m = 0.0;
	get_positions_data.CMincr.E = 0.0;
	get_positions_data.thr_rng = NULL;
	
#ifdef USE_THREADS
	/* create the thread(s) */
	for(t=1;t<NUM_THREADS;t++) {
		get_positions_data_array[t] = get_positions_data;
		get_positions_data_array[t].taskid = t;
		get_positions_data_array[t].thr_rng = gsl_rng_alloc(gsl_rng_taus2);
		gsl_rng_set(get_positions_data_array[t].thr_rng,
				rng_t113_int());
		rc = pthread_create(&threads[t], NULL,
            	            get_positions_loop, 
					(void *) &get_positions_data_array[t]);
		if (rc) {
                  eprintf("return code from pthread_create() is %d\n", rc);
                  exit(-1);
            }
      }
	/* do work on main thread as well */
	get_positions_data_array[0] = get_positions_data;
	get_positions_data_array[0].taskid = 0;
	get_positions_data_array[0].thr_rng = gsl_rng_alloc(gsl_rng_taus2);
	gsl_rng_set(get_positions_data_array[0].thr_rng, rng_t113_int());
	get_positions_loop(&get_positions_data_array[0]);
	/* Free attribute and wait for the other threads */
	/* Update variables when threads join */
      pthread_attr_destroy(&attr);
      for(t=1;t < NUM_THREADS;t++) {
            rc = pthread_join(threads[t], NULL);
            if (rc) {
                  eprintf("return code from pthread_join() is %d\n", rc);
                  exit(-1);
            }
		if (get_positions_data_array[t].max_rad > max_rad) {
			max_rad = get_positions_data_array[t].max_rad;
		}
      }
      for(t=0;t < NUM_THREADS;t++) {
		cenma.m += get_positions_data_array[t].CMincr.m;
		cenma.E += get_positions_data_array[t].CMincr.E;
		gsl_rng_free(get_positions_data_array[t].thr_rng);
	}
	free(get_positions_data_array);
#else
	get_positions_loop(&get_positions_data);
	max_rad = get_positions_data.max_rad;
#endif
	return (max_rad);
}

