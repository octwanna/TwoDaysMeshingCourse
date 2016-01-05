/*     CalculiX - A 3-Dimensional finite element program                   */
/*              Copyright (C) 1998-2013 Guido Dhondt                          */

/*     This program is free software; you can redistribute it and/or     */
/*     modify it under the terms of the GNU General Public License as    */
/*     published by the Free Software Foundation(version 2);    */
/*                    */

/*     This program is distributed in the hope that it will be useful,   */
/*     but WITHOUT ANY WARRANTY; without even the implied warranty of    */ 
/*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the      */
/*     GNU General Public License for more details.                      */

/*     You should have received a copy of the GNU General Public License */
/*     along with this program; if not, write to the Free Software       */
/*     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.         */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "CalculiX.h"
#ifdef SPOOLES
   #include "spooles.h"
#endif
#ifdef SGI
   #include "sgi.h"
#endif
#ifdef TAUCS
   #include "tau.h"
#endif

void expand(double *co, int *nk, int *kon, int *ipkon, char *lakon,
	     int *ne, int *nodeboun, int *ndirboun, double *xboun, int *nboun, 
	     int *ipompc, int *nodempc, double *coefmpc, char *labmpc,
             int *nmpc, int *nodeforc, int *ndirforc,double *xforc, 
             int *nforc, int *nelemload, char *sideload, double *xload,
             int *nload, int *nactdof, int *neq, 
	     int *nmethod, int *ikmpc, int *ilmpc, int *ikboun, int *ilboun,
	     double *elcon, int *nelcon, double *rhcon, int *nrhcon,
	     double *alcon, int *nalcon, double *alzero, int *ielmat,
	     int *ielorien, int *norien, double *orab, int *ntmat_,
	     double *t0,int *ithermal,double *prestr, int *iprestr, 
	     double *vold,int *iperturb, double *sti, int *nzs,  
	     double *adb, double *aub,char *filab, double *eme,
             double *plicon, int *nplicon, double *plkcon,int *nplkcon,
             double *xstate, int *npmat_, char *matname, int *mi,
	     int *ics, double *cs, int *mpcend, int *ncmat_,
             int *nstate_, int *mcs, int *nkon, double *ener,
             char *jobnamec, char *output, char *set, int *nset,int *istartset,
             int *iendset, int *ialset, int *nprint, char *prlab,
             char *prset, int *nener, double *trab, 
             int *inotr, int *ntrans, double *ttime, double *fmpc,
	     int *nev, double **zp, int *iamboun, double *xbounold,
             int *nsectors, int *nm,int *icol,int *irow,int *nzl, int *nam,
             int *ipompcold, int *nodempcold, double *coefmpcold,
             char *labmpcold, int *nmpcold, double *xloadold, int *iamload,
             double *t1old,double *t1,int *iamt1, double *xstiff,int **icolep,
	     int **jqep,int **irowep,int *isolver,
	     int *nzse,double **adbep,double **aubep,int *iexpl,int *ibody,
	     double *xbody,int *nbody,double *cocon,int *ncocon,
	     char* tieset,int* ntie, int **nnnp,int *imddof,int *nmddof,
	     int *imdnode,int *nmdnode,int *imdboun,int *nmdboun,
  	     int *imdmpc,int *nmdmpc, int **izdofp, int *nzdof,int *nherm,
	     double *xmr,double *xmi){

  /* calls the Arnoldi Package (ARPACK) for cyclic symmetry calculations */
  
    char *filabt,*tchar1=NULL,*tchar2=NULL,*tchar3=NULL,lakonl[2]=" \0";

    int *inum=NULL,k,idir,lfin,j,iout=0,index,inode,id,i,idof,im,
        ielas,icmd,kk,l,nkt,icntrl,imag=1,icomplex,kkv,kk6,iterm,
        lprev,ilength,ij,i1,i2,iel,ielset,node,indexe,nope,ml1,nelem,
        *inocs=NULL,*ielcs=NULL,jj,l1,l2,is,nlabel,*nshcon=NULL,
        nodeleft,*noderight=NULL,numnodes,ileft,kflag=2,itr,locdir,
        neqh,j1,nodenew,mass[2]={1,1},stiffness=1,buckling=0,mt=mi[1]+1,
	rhsi=0,intscheme=0,coriolis=0,istep=1,iinc=1,iperturbmass[2],
        *mast1e=NULL,*ipointere=NULL,*irowe=*irowep,*ipobody=NULL,*jqe=*jqep,
	*icole=*icolep,tint=-1,tnstart=-1,tnend=-1,tint2=-1,*nnn=*nnnp,
	noderight_,*izdof=*izdofp,iload,iforc,*iznode=NULL,nznode,ll,ne0,
	*integerglob=NULL,nasym=0,icfd=0,*inomat=NULL;

    long long lint;

    double *stn=NULL,*v=NULL,*temp_array=NULL,*vini=NULL,*csmass=NULL,
        *een=NULL,cam[5],*f=NULL,*fn=NULL,qa[3],*epn=NULL,summass,
        *stiini=NULL,*emn=NULL,*xnormastface=NULL,*emeini=NULL,
	*xstateini=NULL,theta,pi,*coefmpcnew=NULL,t[3],ctl,stl,
	*stx=NULL,*enern=NULL,*xstaten=NULL,*eei=NULL,*enerini=NULL,
	*qfx=NULL,*qfn=NULL,xreal,ximag,*vt=NULL,sum,*aux=NULL,
        *coefright=NULL,*physcon=NULL,coef,a[9],ratio,reltime,*ade=NULL,
        *aue=NULL,*adbe=*adbep,*aube=*aubep,*fext=NULL,*cgr=NULL,
        *shcon=NULL,*springarea=NULL,*z=*zp, *zdof=NULL, *thicke=NULL,
        *doubleglob=NULL,atrab[9],acs[9],diff,fin[3],fout[3],*sumi=NULL,
        *vti=NULL;
    
    /* dummy arguments for the results call */
    
    double *veold=NULL,*accold=NULL,bet,gam,dtime,time;

    pi=4.*atan(1.);
    neqh=neq[1]/2;

    noderight_=10;
    noderight=NNEW(int,noderight_);
    coefright=NNEW(double,noderight_);

    v=NNEW(double,2*mt**nk);
    vt=NNEW(double,mt**nk**nsectors);
    
    fn=NNEW(double,2*mt**nk);
    stn=NNEW(double,12**nk);
    inum=NNEW(int,*nk);
    stx=NNEW(double,6*mi[0]**ne);
    
    nlabel=41;
    filabt=NNEW(char,87*nlabel);
    for(i=1;i<87*nlabel;i++) filabt[i]=' ';
    filabt[0]='U';
    
    temp_array=NNEW(double,neq[1]);
    coefmpcnew=NNEW(double,*mpcend);
    
    nkt=*nsectors**nk;
 
    /* assigning nodes and elements to sectors */
    
    inocs=NNEW(int,*nk);
    ielcs=NNEW(int,*ne);
    ielset=cs[12];
    if((*mcs!=1)||(ielset!=0)){
	for(i=0;i<*nk;i++) inocs[i]=-1;
	for(i=0;i<*ne;i++) ielcs[i]=-1;
    }
    csmass=NNEW(double,*mcs);
    if(*mcs==1) csmass[0]=1.;
    
    for(i=0;i<*mcs;i++){
	is=cs[17*i];
	//	if(is==1) continue;
	ielset=cs[17*i+12];
	if(ielset==0) continue;
	for(i1=istartset[ielset-1]-1;i1<iendset[ielset-1];i1++){
	    if(ialset[i1]>0){
		iel=ialset[i1]-1;
		if(ipkon[iel]<0) continue;
		ielcs[iel]=i;
		indexe=ipkon[iel];
		if(*mcs==1){
		  if(strcmp1(&lakon[8*iel+3],"2")==0)nope=20;
		  else if (strcmp1(&lakon[8*iel+3],"8")==0)nope=8;
		  else if (strcmp1(&lakon[8*iel+3],"10")==0)nope=10;
		  else if (strcmp1(&lakon[8*iel+3],"4")==0)nope=4;
		  else if (strcmp1(&lakon[8*iel+3],"15")==0)nope=15;
		  else if (strcmp1(&lakon[8*iel+3],"6")==0)nope=6;
		  else if (strcmp1(&lakon[8*iel],"ES")==0){
		      lakonl[0]=lakon[8*iel+7];
		      nope=atoi(lakonl)+1;}
		  else continue;
		}else{
		  nelem=iel+1;
		  FORTRAN(calcmass,(ipkon,lakon,kon,co,mi,&nelem,ne,thicke,
                        ielmat,&nope,t0,t1,rhcon,nrhcon,ntmat_,
			ithermal,&csmass[i]));
		}
		for(i2=0;i2<nope;++i2){
		    node=kon[indexe+i2]-1;
		    inocs[node]=i;
		}
	    }
	    else{
		iel=ialset[i1-2]-1;
		do{
		    iel=iel-ialset[i1];
		    if(iel>=ialset[i1-1]-1) break;
		    if(ipkon[iel]<0) continue;
		    ielcs[iel]=i;
		    indexe=ipkon[iel];
		    if(*mcs==1){
		      if(strcmp1(&lakon[8*iel+3],"2")==0)nope=20;
		      else if (strcmp1(&lakon[8*iel+3],"8")==0)nope=8;
		      else if (strcmp1(&lakon[8*iel+3],"10")==0)nope=10;
		      else if (strcmp1(&lakon[8*iel+3],"4")==0)nope=4;
		      else if (strcmp1(&lakon[8*iel+3],"15")==0)nope=15;
		      else {nope=6;}
		    }else{
		      nelem=iel+1;
		      FORTRAN(calcmass,(ipkon,lakon,kon,co,mi,&nelem,ne,thicke,
                        ielmat,&nope,t0,t1,rhcon,nrhcon,ntmat_,
			ithermal,&csmass[i]));
		    }
		    for(i2=0;i2<nope;++i2){
			node=kon[indexe+i2]-1;
			inocs[node]=i;
		    }
		}while(1);
	    }
	} 
//	printf("expand.c mass = %d,%e\n",i,csmass[i]);
    }

    /* copying imdnode into iznode 
       iznode contains the nodes in which output is requested and
       the nodes in which loading is applied */

    iznode=NNEW(int,*nk);
    for(j=0;j<*nmdnode;j++){iznode[j]=imdnode[j];}
    nznode=*nmdnode;

/* expanding imddof, imdnode, imdboun and imdmpc */

    for(i=1;i<*nsectors;i++){
	for(j=0;j<*nmddof;j++){
	    imddof[i**nmddof+j]=imddof[j]+i*neqh;
	}
	for(j=0;j<*nmdnode;j++){
	    imdnode[i**nmdnode+j]=imdnode[j]+i**nk;
	}
	for(j=0;j<*nmdboun;j++){
	    imdboun[i**nmdboun+j]=imdboun[j]+i**nboun;
	}
	for(j=0;j<*nmdmpc;j++){
	    imdmpc[i**nmdmpc+j]=imdmpc[j]+i**nmpc;
	}
    }
    (*nmddof)*=(*nsectors);
    (*nmdnode)*=(*nsectors);
    (*nmdboun)*=(*nsectors);
    (*nmdmpc)*=(*nsectors);

/* creating a field with the degrees of freedom in which the eigenmodes
   are needed:
   1. all dofs in which the solution is needed (=imddof)
   2. all dofs in which loading was applied
 */	

    izdof=NNEW(int,neqh**nsectors);
    for(j=0;j<*nmddof;j++){izdof[j]=imddof[j];}
    *nzdof=*nmddof;
    
    /* generating the coordinates for the other sectors */
    
    icntrl=1;
    
    FORTRAN(rectcyl,(co,v,fn,stn,qfn,een,cs,nk,&icntrl,t,filabt,&imag,mi,emn));
    
    for(jj=0;jj<*mcs;jj++){
	is=(int)(cs[17*jj]+0.5);
	for(i=1;i<is;i++){
	    
	    theta=i*2.*pi/cs[17*jj];
	    
	    for(l=0;l<*nk;l++){
		if(inocs[l]==jj){
		    co[3*l+i*3**nk]=co[3*l];
		    co[1+3*l+i*3**nk]=co[1+3*l]+theta;
		    co[2+3*l+i*3**nk]=co[2+3*l];
		    if(*ntrans>0) inotr[2*l+i*2**nk]=inotr[2*l];
		}
	    }
	    for(l=0;l<*nkon;l++){kon[l+i**nkon]=kon[l]+i**nk;}
	    for(l=0;l<*ne;l++){
		if(ielcs[l]==jj){
		    if(ipkon[l]>=0){
			ipkon[l+i**ne]=ipkon[l]+i**nkon;
			ielmat[mi[2]*(l+i**ne)]=ielmat[mi[2]*l];
			if(*norien>0) ielorien[l+i**ne]=ielorien[l];
			for(l1=0;l1<8;l1++){
			    l2=8*l+l1;
			    lakon[l2+i*8**ne]=lakon[l2];
			}
		    }else{
			ipkon[l+i**ne]=ipkon[l];
		    }	
		}
	    }
	}
    }
    
    icntrl=-1;
    
    FORTRAN(rectcyl,(co,vt,fn,stn,qfn,een,cs,&nkt,&icntrl,t,filabt,&imag,mi,emn));

/* expand nactdof */

    for(i=1;i<*nsectors;i++){
	lint=i*mt**nk;
	for(j=0;j<mt**nk;j++){
	    if(nactdof[j]!=0){
		nactdof[lint+j]=nactdof[j]+i*neqh;
	    }else{
		nactdof[lint+j]=0;
	    }
	}
    }
    
/* copying the boundary conditions
   (SPC's must be defined in cylindrical coordinates) */
    
    for(i=1;i<*nsectors;i++){
	for(j=0;j<*nboun;j++){
	    nodeboun[i**nboun+j]=nodeboun[j]+i**nk;
	    ndirboun[i**nboun+j]=ndirboun[j];
	    xboun[i**nboun+j]=xboun[j];
	    xbounold[i**nboun+j]=xbounold[j];
	    if(*nam>0) iamboun[i**nboun+j]=iamboun[j];
	    ikboun[i**nboun+j]=ikboun[j]+8*i**nk;
	    ilboun[i**nboun+j]=ilboun[j]+i**nboun;
	}
    }
    
    /* distributed loads */
    
    for(i=0;i<*nload;i++){
	if(nelemload[2*i+1]<*nsectors){
	    nelemload[2*i]+=*ne*nelemload[2*i+1];
	}else{
	    nelemload[2*i]+=*ne*(nelemload[2*i+1]-(*nsectors));
	}
	iload=i+1;
	FORTRAN(addizdofdload,(nelemload,sideload,ipkon,kon,lakon,
		nactdof,izdof,nzdof,mi,&iload,iznode,&nznode,nk,
		imdnode,nmdnode));
    }

    /* body loads */

    if(*nbody>0){
	printf("*ERROR in expand: body loads are not allowed for modal dynamics\n and steady state dynamics calculations in cyclic symmetric structures\n\n");
	FORTRAN(stop,());
    }

  /*  sorting the elements with distributed loads */

    if(*nload>0){
	if(*nam>0){
	    FORTRAN(isortiddc2,(nelemload,iamload,xload,xloadold,sideload,nload,&kflag));
	}else{
	    FORTRAN(isortiddc1,(nelemload,xload,xloadold,sideload,nload,&kflag));
	}
    }
    
    /* point loads */
    
/*    for(i=0;i<*nforc;i++){
	if(nodeforc[2*i+1]<*nsectors){
	    nodeforc[2*i]+=*nk*nodeforc[2*i+1];
	}else{
	    nodeforc[2*i]+=*nk*(nodeforc[2*i+1]-(*nsectors));
	}
	iforc=i+1;
	FORTRAN(addizdofcload,(nodeforc,ndirforc,nactdof,mi,izdof,
	        nzdof,&iforc,iznode,&nznode,nk,imdnode,nmdnode,xforc));
		}*/

    i=0;
    while(i<*nforc){
	node=nodeforc[2*i];
	
	/* checking for a cylindrical transformation;
	   comparison with the cyclic symmetry system */
	
	itr=inotr[2*node-2];

	if(itr==0){

            /* carthesian coordinate system */

	    if(nodeforc[2*i+1]<*nsectors){
		nodeforc[2*i]+=*nk*nodeforc[2*i+1];
	    }else{
		nodeforc[2*i]+=*nk*(nodeforc[2*i+1]-(*nsectors));
	    }
	    i++;iforc=i;
	    FORTRAN(addizdofcload,(nodeforc,ndirforc,nactdof,mi,izdof,
		    nzdof,&iforc,iznode,&nznode,nk,imdnode,nmdnode,xforc));
	}else{

	    /* cylindrical coordinate system */	
	    
	    FORTRAN(transformatrix,(&trab[7*(itr-1)],&co[3*(node-1)],atrab));
	    FORTRAN(transformatrix,(&cs[5],&co[3*(node-1)],acs));
	    diff=0.; for(j=0;j<9;j++) diff+=(atrab[j]-acs[j])*(atrab[j]-acs[j]);
	    
	    if((ndirforc[i]!=1)||
	       (nodeforc[2*i+2]!=node)||(ndirforc[i+1]!=2)||
	       (nodeforc[2*i+4]!=node)||(ndirforc[i+2]!=3)||
	       ((diff>1.e-10)&&(fabs(diff-8.)>1.e-10))){
		printf("*ERROR: forces in a modal dynamic or steady state dynamics\n");
		printf("        calculation with cyclic symmetry must be defined in\n");
		printf("        the cyclic symmetric cylindrical coordinate system\n");
		printf("        force at fault is applied in node %d\n",node);
		FORTRAN(stop,());
	    }
	    
	    /* changing the complete force in the node in the basis sector from
	       the global rectangular system into the cylindrical system */
	    
	    fin[0]=xforc[i];
	    fin[1]=xforc[i+1];
	    fin[2]=xforc[i+2];
	    icntrl=2;
	    FORTRAN(rectcyltrfm,(&node,co,cs,&icntrl,fin,fout));
	    
	    /* new node number (= node number in the target sector) */
	    
	    if(nodeforc[2*i+1]<*nsectors){
		nodeforc[2*i]+=*nk*nodeforc[2*i+1];
	    }else{
		nodeforc[2*i]+=*nk*(nodeforc[2*i+1]-(*nsectors));
	    }
	    nodeforc[2*i+2]=nodeforc[2*i];
	    nodeforc[2*i+4]=nodeforc[2*i];
	    
	    /* changing the complete force in the node in the target sector from
	       the cylindrical system into the global rectangular system */
	    
	    node=nodeforc[2*i];
	    fin[0]=fout[0];
	    fin[1]=fout[1];
	    fin[2]=fout[2];
	    icntrl=-2;
	    FORTRAN(rectcyltrfm,(&node,co,cs,&icntrl,fin,fout));
	    xforc[i]=fout[0];
	    xforc[i+1]=fout[1];
	    xforc[i+2]=fout[2];
	    
	    /* storing the node and the dof into iznode and izdof */
	    
	    for(j=0;j<3;j++){
		i++;iforc=i;
		FORTRAN(addizdofcload,(nodeforc,ndirforc,nactdof,mi,izdof,
			nzdof,&iforc,iznode,&nznode,nk,imdnode,nmdnode,xforc));
	    }
	}
    }
    
    /* loop over all eigenvalues; the loop starts from the highest eigenvalue
       so that the reuse of z is not a problem
       z before: real and imaginary part for a segment for all eigenvalues
       z after: real part for all segments for all eigenvalues */

    if(*nherm==1){
	zdof=NNEW(double,(long long)*nev**nzdof);
    }else{
	zdof=NNEW(double,(long long)2**nev**nzdof);
	sumi=NNEW(double,*nev);
    }

    lfin=0;
    for(j=*nev-1;j>-1;--j){
	lint=2*j*neqh;

	/* calculating the cosine and sine of the phase angle */

	for(jj=0;jj<*mcs;jj++){
	    theta=nm[j]*2.*pi/cs[17*jj];
	    cs[17*jj+14]=cos(theta);
	    cs[17*jj+15]=sin(theta);
	}
	
	/* generating the cyclic MPC's (needed for nodal diameters
	   different from 0 */
	
	eei=NNEW(double,6*mi[0]**ne);

	DMEMSET(v,0,2*mt**nk,0.);
	
	for(k=0;k<2*neqh;k+=neqh){
	    
	    for(i=0;i<6*mi[0]**ne;i++){eme[i]=0.;}
	    
	    if(k==0) {kk=0;kkv=0;kk6=0;}
	    else {kk=*nk;kkv=mt**nk;kk6=6**nk;}
	    for(i=0;i<*nmpc;i++){
		index=ipompc[i]-1;
		/* check whether thermal mpc */
		if(nodempc[3*index+1]==0) continue;
		coefmpcnew[index]=coefmpc[index];
		while(1){
		    index=nodempc[3*index+2];
		    if(index==0) break;
		    index--;
		    
		    icomplex=0;
		    inode=nodempc[3*index];
		    if(strcmp1(&labmpc[20*i],"CYCLIC")==0){
			icomplex=atoi(&labmpc[20*i+6]);}
		    else if(strcmp1(&labmpc[20*i],"SUBCYCLIC")==0){
			for(ij=0;ij<*mcs;ij++){
			    lprev=cs[ij*17+13];
			    ilength=cs[ij*17+3];
			    FORTRAN(nident,(&ics[lprev],&inode,&ilength,&id));
			    if(id!=0){
				if(ics[lprev+id-1]==inode){icomplex=ij+1;break;}
			    }
			}
		    }
		    
		    if(icomplex!=0){
			idir=nodempc[3*index+1];
			idof=nactdof[mt*(inode-1)+idir]-1;
			if(idof==-1){xreal=1.;ximag=1.;}
			else{xreal=z[lint+idof];ximag=z[lint+idof+neqh];}
			if(k==0) {
			    if(fabs(xreal)<1.e-30)xreal=1.e-30;
			    coefmpcnew[index]=coefmpc[index]*
				(cs[17*(icomplex-1)+14]+
                                 ximag/xreal*cs[17*(icomplex-1)+15]);}
			else {
			    if(fabs(ximag)<1.e-30)ximag=1.e-30;
			    coefmpcnew[index]=coefmpc[index]*
				(cs[17*(icomplex-1)+14]-
                                 xreal/ximag*cs[17*(icomplex-1)+15]);}
		    }
		    else{coefmpcnew[index]=coefmpc[index];}
		}
	    }
	    
	    results(co,nk,kon,ipkon,lakon,ne,&v[kkv],&stn[kk6],inum,
	      stx,elcon,
	      nelcon,rhcon,nrhcon,alcon,nalcon,alzero,ielmat,ielorien,
	      norien,orab,ntmat_,t0,t0,ithermal,
	      prestr,iprestr,filab,eme,&emn[kk6],&een[kk6],iperturb,
	      f,&fn[kkv],nactdof,&iout,qa,vold,&z[lint+k],
	      nodeboun,ndirboun,xboun,nboun,ipompc,
	      nodempc,coefmpcnew,labmpc,nmpc,nmethod,cam,&neqh,veold,accold,
	      &bet,&gam,&dtime,&time,ttime,plicon,nplicon,plkcon,nplkcon,
	      xstateini,xstiff,xstate,npmat_,epn,matname,mi,&ielas,&icmd,
	      ncmat_,nstate_,stiini,vini,ikboun,ilboun,ener,&enern[kk],emeini,
	      xstaten,eei,enerini,cocon,ncocon,set,nset,istartset,iendset,
	      ialset,nprint,prlab,prset,qfx,qfn,trab,inotr,ntrans,fmpc,
	      nelemload,nload,ikmpc,ilmpc,&istep,&iinc,springarea,&reltime,
              &ne0,xforc,nforc,thicke,xnormastface,shcon,nshcon,
              sideload,xload,xloadold,&icfd,inomat);
	    
	}
	free(eei);

	/* mapping the results to the other sectors */

	if(*nherm!=1)vti=NNEW(double,mt**nk**nsectors);
	
	icntrl=2;imag=1;
	
	FORTRAN(rectcylexp,(co,v,fn,stn,qfn,een,cs,nk,&icntrl,t,filabt,&imag,mi,
			    iznode,&nznode,nsectors,nk,emn));
	
	/* basis sector */

	for(ll=0;ll<nznode;ll++){
	    l1=iznode[ll]-1;
	    for(l2=0;l2<mt;l2++){
		l=mt*l1+l2;
		vt[l]=v[l];
		if(*nherm!=1)vti[l]=v[l+mt**nk];
	    }
	}

	/* other sectors */

	for(jj=0;jj<*mcs;jj++){
	    ilength=cs[17*jj+3];
	    lprev=cs[17*jj+13];
	    for(i=1;i<*nsectors;i++){
		
		theta=i*nm[j]*2.*pi/cs[17*jj];
		ctl=cos(theta);
		stl=sin(theta);
		
		for(ll=0;ll<nznode;ll++){
		    l1=iznode[ll]-1;
		    if(inocs[l1]==jj){
			
			/* check whether node lies on axis */
			
			ml1=-l1-1;
			FORTRAN(nident,(&ics[lprev],&ml1,&ilength,&id));
			if(id!=0){
			    if(ics[lprev+id-1]==ml1){
				for(l2=0;l2<mt;l2++){
				    l=mt*l1+l2;
				    vt[l+mt**nk*i]=v[l];
				    if(*nherm!=1)vti[l+mt**nk*i]=v[l+mt**nk];
				}
				continue;
			    }
			}
			for(l2=0;l2<mt;l2++){
			    l=mt*l1+l2;
			    vt[l+mt**nk*i]=ctl*v[l]-stl*v[l+mt**nk];
			    if(*nherm!=1)vti[l+mt**nk*i]=stl*v[l]+ctl*v[l+mt**nk];
			}
		    }
		}
	    }
	}
	
	icntrl=-2;imag=0;
	
	FORTRAN(rectcylexp,(co,vt,fn,stn,qfn,een,cs,&nkt,&icntrl,t,filabt,
			    &imag,mi,iznode,&nznode,nsectors,nk,emn));
	
/* storing the displacements into the expanded eigenvectors */

	for(ll=0;ll<nznode;ll++){
	    i=iznode[ll]-1;
//	for(i=0;i<*nk;i++){
	    for(j1=0;j1<mt;j1++){

		for(k=0;k<*nsectors;k++){
		    /* C-convention */
		    idof=nactdof[mt*(k**nk+i)+j1]-1;
		    if(idof!=-1){
			FORTRAN(nident,(izdof,&idof,nzdof,&id));
			if(id!=0){
			    if(izdof[id-1]==idof){
				if(*nherm==1){
				    zdof[(long long)j**nzdof+id-1]=vt[k*mt**nk+mt*i+j1];
				}else{
				    zdof[(long long)2*j**nzdof+id-1]=vt[k*mt**nk+mt*i+j1];
				    zdof[(long long)(2*j+1)**nzdof+id-1]=vti[k*mt**nk+mt*i+j1];
				}
			    }
			}
		    }
		}
	    }	    
	}

	if(*nherm!=1) free(vti);
	
/* normalizing the eigenvectors with the mass */

/*	if (nm[j]==0||(nm[j]==(int)((cs[0]/2))&&(fmod(cs[0],2.)==0.)))
                 {sum=sqrt(cs[0]);}
		 else{sum=sqrt(cs[0]/2);}*/

	sum=0.;
	summass=0.;
	for(i=0;i<*mcs;i++){
	  if (nm[j]==0||(nm[j]==(int)((cs[17*i]/2))&&(fmod(cs[17*i],2.)==0.))){
	    sum+=cs[17*i]*csmass[i];
	  }else{
	    sum+=cs[17*i]*csmass[i]/2.;
	  }
	  summass+=csmass[i];
	}
	if(fabs(summass)>1.e-20){
	  sum=sqrt(sum/summass);
	}else{
	  printf("*ERROR in expand.c: total mass of structure is zero\n");
	  printf("       maybe no element sets were specified for the\n");
	  printf("       cyclic symmetry ties\n");
	  FORTRAN(stop,());
	}

	if(*nherm==1){
	    for(i=0;i<*nzdof;i++){zdof[(long long)j**nzdof+i]/=sum;}
	}else{
	    for(i=0;i<*nzdof;i++){zdof[(long long)(2*j)**nzdof+i]/=sum;}
	    for(i=0;i<*nzdof;i++){zdof[(long long)(2*j+1)**nzdof+i]/=sum;}
	    sumi[j]=sqrt(sum);
	}
    }
  
/* copying zdof into z */
  
    if(*nherm==1){
	RENEW(z,double,(long long)*nev**nzdof);
	memcpy(&z[0],&zdof[0],(long long)sizeof(double)**nev**nzdof);
    }else{
	RENEW(z,double,(long long)2**nev**nzdof);
	memcpy(&z[0],&zdof[0],(long long)sizeof(double)*2**nev**nzdof);
	for(i=0;i<*nev;i++){
	    for(j=0;j<*nev;j++){
		xmr[i**nev+j]/=(sumi[i]*sumi[j]);
		xmi[i**nev+j]/=(sumi[i]*sumi[j]);
	    }
	}
	free(sumi);
    }
    free(zdof);

/* copying the multiple point constraints */

    *nmpc=0;
    *mpcend=0;
    for(i=0;i<*nsectors;i++){
	if(i==0){
	    ileft=*nsectors-1;
	}else{
	    ileft=i-1;
	}
	for(j=0;j<*nmpcold;j++){
	    if(noderight_>10){
		noderight_=10;
		RENEW(noderight,int,noderight_);
		RENEW(coefright,double,noderight_);
	    }
	    ipompc[*nmpc]=*mpcend+1;
	    ikmpc[*nmpc]=ikmpc[j]+8*i**nk;
	    ilmpc[*nmpc]=ilmpc[j]+i**nmpcold;
	    strcpy1(&labmpc[20**nmpc],&labmpcold[20*j],20);
	    if(strcmp1(&labmpcold[20*j],"CYCLIC")==0){
		index=ipompcold[j]-1;
		nodeleft=nodempcold[3*index];
		idir=nodempcold[3*index+1];
		index=nodempcold[3*index+2]-1;
		numnodes=0;
		do{
		    node=nodempcold[3*index];
		    if(nodempcold[3*index+1]==idir){
			noderight[numnodes]=node;
			coefright[numnodes]=coefmpcold[index];
			numnodes++;
			if(numnodes>=noderight_){
			    noderight_=(int)(1.5*noderight_);
			    RENEW(noderight,int,noderight_);
			    RENEW(coefright,double,noderight_);
			}
		    }
		    index=nodempcold[3*index+2]-1;
		    if(index==-1) break;
		}while(1);
		if(numnodes>0){
		    sum=0.;
		    for(k=0;k<numnodes;k++){
			sum+=coefright[k];
		    }
		    for(k=0;k<numnodes;k++){
			coefright[k]/=sum;
		    }
		}else{coefright[0]=1.;}
		nodempc[3**mpcend]=nodeleft+i**nk;
		nodempc[3**mpcend+1]=idir;
		nodempc[3**mpcend+2]=*mpcend+2;
		coefmpc[*mpcend]=1.;
		for(k=0;k<numnodes;k++){
		    (*mpcend)++;
		    nodempc[3**mpcend]=noderight[k]+ileft**nk;
		    nodempc[3**mpcend+1]=idir;
		    nodempc[3**mpcend+2]=*mpcend+2;
		    coefmpc[*mpcend]=-coefright[k];
		}
		nodempc[3**mpcend+2]=0;
		(*mpcend)++;
	    }else{
		index=ipompcold[j]-1;
		iterm=0;
		do{
		    iterm++;
		    node=nodempcold[3*index];
		    idir=nodempcold[3*index+1];
		    coef=coefmpcold[index];

		    /* check whether SUBCYCLIC MPC: if the current node
                       is an independent node of a CYCLIC MPC, the
                       node in the new MPC should be the cylic previous
                       one */

		    nodenew=node+i**nk;
		    if(strcmp1(&labmpcold[20*j],"SUBCYCLIC")==0){
			for(ij=0;ij<*mcs;ij++){
			    lprev=cs[ij*17+13];
			    ilength=cs[ij*17+3];
			    FORTRAN(nident,(&ics[lprev],&node,&ilength,&id));
			    if(id!=0){
				if(ics[lprev+id-1]==node){
				    nodenew=node+ileft**nk;
				    break;
				}
			    }
			}
		    }
		    
		    /* modification of the MPC coefficients if
                       cylindrical coordinate system is active
                       it is assumed that all terms in the MPC are
                       either in the radial, the circumferential
                       or axial direction  */
		    
		    if(*ntrans<=0){itr=0;}
		    else if(inotr[2*node-2]==0){itr=0;}
		    else{itr=inotr[2*node-2];}
		    
		    if(iterm==1) locdir=-1;
		    
		    if((itr!=0)&&(idir!=0)){
			if(trab[7*itr-1]<0){
			    FORTRAN(transformatrix,(&trab[7*itr-7],
						    &co[3*node-3],a));
			    if(iterm==1){
				for(k=0;k<3;k++){
				    if(fabs(a[3*k+idir-1]-coef)<1.e-10){
					FORTRAN(transformatrix,(&trab[7*itr-7],
								&co[3*nodenew-3],a));
					coef=a[3*k+idir-1];
					locdir=k;
					break;
				    }
				    if(fabs(a[3*k+idir-1]+coef)<1.e-10){
					FORTRAN(transformatrix,(&trab[7*itr-7],
								&co[3*nodenew-3],a));
					coef=-a[3*k+idir-1];
					locdir=k;
					break;
				    }
				}
			    }else{
				if(locdir!=-1){
				    if(fabs(a[3*locdir+idir-1])>1.e-10){
					ratio=coef/a[3*locdir+idir-1];
				    }else{ratio=0.;}
				    FORTRAN(transformatrix,(&trab[7*itr-7],
							    &co[3*nodenew-3],a));
				    coef=ratio*a[3*locdir+idir-1];
				}
			    }		
			}
		    }
		    
		    nodempc[3**mpcend]=nodenew;
		    nodempc[3**mpcend+1]=idir;
		    coefmpc[*mpcend]=coef;
		    index=nodempcold[3*index+2]-1;
		    if(index==-1) break;
		    nodempc[3**mpcend+2]=*mpcend+2;
		    (*mpcend)++;
		}while(1);
		nodempc[3**mpcend+2]=0;
		(*mpcend)++;
	    }
	    (*nmpc)++;
	}
    }

    /* copying the temperatures */

    if(*ithermal!=0){
	for(i=1;i<*nsectors;i++){
	    lint=i**nk;
	    for(j=0;j<*nk;j++){
		t0[lint+j]=t0[j];
		t1old[lint+j]=t1old[j];
		t1[lint+j]=t1[j];
	    }
	}
	if(*nam>0){
	    for(i=1;i<*nsectors;i++){
		lint=i**nk;
		for(j=0;j<*nk;j++){
		    iamt1[lint+j]=iamt1[j];
		}
	    }
	}
    }

    /* copying the contact definition */

    if(*nmethod==4){
      
      /* first find the startposition to append the expanded contact fields*/
      
      for(j=0; j<*nset; j++){
	if(iendset[j]>tint){
	  tint=iendset[j];
	}
      }
      tint++;
      /* now append and expand the contact definitons*/
      tchar1=NNEW(char,81);
      tchar2=NNEW(char,81);
      tchar3=NNEW(char,81);
      for(i=0; i<*ntie; i++){
	if(tieset[i*(81*3)+80]=='C'){
	  memcpy(tchar2,&tieset[i*(81*3)+81],81);
	  tchar2[80]='\0';
	  memcpy(tchar3,&tieset[i*(81*3)+81+81],81);
	  tchar3[80]='\0';
	  //a contact constraint was found, so append and expand the information
	  for(j=0; j<*nset; j++){
	    memcpy(tchar1,&set[j*81],81);
	    tchar1[80]='\0';
	    if(strcmp(tchar1,tchar2)==0){
	      /* dependent nodal surface was found,copy the original information first */
	      tnstart=tint;
	      for(k=0; k<iendset[j]-istartset[j]+1; k++){
		ialset[tint-1]=ialset[istartset[j]-1+k];
		tint++;
	      }
	      /* now append the expanded information */
	      for(l=1; l<*nsectors; l++){
		for(k=0; k<iendset[j]-istartset[j]+1; k++){
		  ialset[tint-1]=(ialset[istartset[j]-1+k]!=-1)?ialset[istartset[j]-1+k]+*nk*l:-1;
		  tint++;
		}
	      }
	      tnend=tint-1;
	      /* now replace the information in istartset and iendset*/
	      istartset[j]=tnstart;
	      iendset[j]=tnend;
	    }
	    else if(strcmp(tchar1,tchar3)==0){
	      /* independent element face surface was found */
	      tnstart=tint;
	      for(k=0; k<iendset[j]-istartset[j]+1; k++){
		ialset[tint-1]=ialset[istartset[j]-1+k];
		tint++;
	      }
	      /* now append the expanded information*/
	      for(l=1; l<*nsectors; l++){
		for(k=0; k<iendset[j]-istartset[j]+1; k++){
		  tint2=((int)(ialset[istartset[j]-1+k]))/10;
		  ialset[tint-1]=(ialset[istartset[j]-1+k]!=-1)?(tint2+*ne*l)*10+(ialset[istartset[j]-1+k]-(tint2*10)):-1;
		  tint++;
		}
	      }
	      tnend=tint-1;
	      /* now replace the information in istartset and iendset*/
	      istartset[j]=tnstart;
	      iendset[j]=tnend;
	    }
	  }
	}
      }
      free(tchar1);
      free(tchar2);
      free(tchar3);
    }    
    
    *nk=nkt;
    (*ne)*=(*nsectors);
    (*nkon)*=(*nsectors);
    (*nboun)*=(*nsectors);
    neq[1]=neqh**nsectors;
      
      /*create the new mass-matrix*/
      /*create the matrix structure*/

      /* next major section is not used: no mass matrix nor stiffness matrix
         is calculated for cyclic symmetric structures; thus, the initial
         conditions have to be zero and no nonzero boundary conditions are
         allowed */

//    if(*nmethod==4){
    if(*nmethod==10){

      /* expanding nnn */

      RENEW(nnn,int,nkt);
      l=0;
      for(jj=*nk; jj<nkt; jj++){
	if((jj % *nk)==0){
	  l++;
	}
	nnn[jj]=nnn[jj-(l**nk)]+l**nk;
      }

      nzse[0]=nzs[0];
      nzse[1]=nzs[1];
      nzse[2]=nzs[2];
      
      mast1e=NNEW(int,nzse[1]);
      irowe=NNEW(int,nzse[1]);
      icole=NNEW(int,4**nk);
      jqe=NNEW(int,4**nk+1);
      ipointere=NNEW(int,4**nk);
      
      mastruct(nk,kon,ipkon,lakon,ne,nodeboun,ndirboun,nboun,ipompc,
	       nodempc,nmpc,nactdof,icole,jqe,&mast1e,&irowe,isolver,
	       neq,nnn,ikmpc,ilmpc,ipointere,nzse,nmethod,ithermal,
	       ikboun,ilboun,iperturb,mi);
      
      free(mast1e);free(ipointere);

      /* fill the matrix */
      
      /* stiffness matrix */

      ade=NNEW(double,neq[1]);
      aue=NNEW(double,nzse[2]);
      
      /* mass matrix */

      adbe=NNEW(double,neq[1]);
      aube=NNEW(double,nzse[1]);
      
      fext=NNEW(double,neq[1]);
      
      /* only the mass matrix must be calculated. Within mafillsm this
         cannot be done without calculating the stiffness matrix at the
         same time; to reduce computational cost the stiffness matrix
         is calculated in a linear way, i.e. without large deformation
         and stress stiffness; therefore, a new variable iperturbmass
         is introduced */

      iperturbmass[0]=iperturb[0];
      iperturbmass[1]=0;

      FORTRAN(mafillsm,(co,nk,kon,ipkon,lakon,ne,nodeboun,ndirboun,xboun,nboun,
	      ipompc,nodempc,coefmpc,nmpc,nodeforc,ndirforc,xforc,
	      nforc,nelemload,sideload,xload,nload,xbody,ipobody,nbody,
	      cgr,ade,aue,fext,nactdof,icole,jqe,irowe,neq,nzl,nmethod,
	      ikmpc,ilmpc,ikboun,ilboun,
	      elcon,nelcon,rhcon,nrhcon,alcon,nalcon,alzero,ielmat,
	      ielorien,norien,orab,ntmat_,
	      t0,t0,ithermal,prestr,iprestr,vold,iperturbmass,sti,
	      nzse,stx,adbe,aube,iexpl,plicon,nplicon,plkcon,nplkcon,
	      xstiff,npmat_,&dtime,matname,mi,
	      ncmat_,mass,&stiffness,&buckling,&rhsi,&intscheme,
	      physcon,shcon,nshcon,cocon,ncocon,ttime,&time,&istep,&iinc,
	      &coriolis,ibody,xloadold,&reltime,veold,springarea,nstate_,
              xstateini,xstate,thicke,xnormastface,integerglob,doubleglob,
	      tieset,istartset,iendset,ialset,ntie,&nasym));
      
      
      free(fext);
      
      free(ade);free(aue);
      
      *adbep=adbe;*aubep=aube;*irowep=irowe;*icolep=icole;*jqep=jqe;*nnnp=nnn;
      
    }

    *zp=z;*izdofp=izdof;
    
    free(temp_array);free(coefmpcnew);free(noderight);free(coefright);
    free(v);free(vt);free(fn);free(stn);free(inum);free(stx);
    free(inocs);free(ielcs);free(filabt);free(iznode);free(csmass);

    return;
}
