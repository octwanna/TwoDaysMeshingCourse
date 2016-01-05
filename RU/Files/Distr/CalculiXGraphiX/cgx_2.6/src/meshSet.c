/* --------------------------------------------------------------------  */
/*                          CALCULIX                                     */
/*                   - GRAPHICAL INTERFACE -                             */
/*                                                                       */
/*     A 3-dimensional pre- and post-processor for finite elements       */
/*              Copyright (C) 1996 Klaus Wittig                          */
/*                                                                       */
/*     This program is free software; you can redistribute it and/or     */
/*     modify it under the terms of the GNU General Public License as    */
/*     published by the Free Software Foundation; version 2 of           */
/*     the License.                                                      */
/*                                                                       */
/*     This program is distributed in the hope that it will be useful,   */
/*     but WITHOUT ANY WARRANTY; without even the implied warranty of    */ 
/*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the      */
/*     GNU General Public License for more details.                      */
/*                                                                       */
/*     You should have received a copy of the GNU General Public License */
/*     along with this program; if not, write to the Free Software       */
/*     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.         */
/* --------------------------------------------------------------------  */

/* TO DO  */
/* -mittelknoten bei quadratischen elementen koennen aussermittig liegen                    */
/*  insbesondere bei nurbs-proj. verschiebung auf kreisbogen ist notwendig.                 */
/*                                                                                          */
/*                                                                                          */
/* -5-Seitige Bodies sollten noch nicht vernetzbar sein, da die surfnodes von surf2 noch    */
/*  nicht entsprechend den geaenderten u,v koordinaten umsortiert werden, wenn diese neu    */
/*  angelegt werden musste, siehe Zeile:                                                    */
/*     for(j=0; j<surf[ss[1]].nn; j++) surf[ss[1]].nod[j]=surf[s].nod[j];                   */
/*  sie sind nur dank des meshimprovers ok. Aber unter gewissen umstaenden reicht das nicht */
/*                                                                                          */
/* -7 sided bodies: bodyFrom7Surfs hier wird unter umstaenden noch nicht immer geprueft ob  */
/*  ein linienindex wirkich einer linie oder einer lcmb zugeordnet werden muss. Die variable */
/*  lm2[] ist hier verdaechtig */

#include <cgx.h>

#define MIN_LINELENGTH 1e-9
#define INTERPOL_MODE 0

#define TEST          0
#define TEST1         0
#define TEST2         0



extern double     gtol;

extern char  printFlag;                   /* printf 1:on 0:off */
extern char  fillSurfFlag;                /* 1: generate triangles for surface-rendering and projection */
extern char  buffer[MAX_LINE_LENGTH]; 

FILE *handle;      /* for debugging */

char namebuf[MAX_LINE_LENGTH];

extern Scale     scale[1];
extern Summen    anz[1];
extern Nodes     *node;
extern Elements  *e_enqire;
extern NodeBlocks *nBlock;
extern Datasets *lcase;

extern Alias     *alias;
extern Sets      *set;
extern Points    *point;
extern Lines     *line;
extern Lcmb      *lcmb;
extern Gsur      *surf;
extern Gbod      *body;
extern Nurbs     *nurbs;
extern Shapes    *shape;
extern SumGeo    anzGeo[1];

/* additional entities from setFunktions */
extern SpecialSet specialset[1];

extern int       setall;                /* setNr of the default set "all" */

/* preliminary nodes, they will be deleded after the elements are created */
Summen    apre[1];
Nodes     *npre=NULL;

/* relation of node generated on entity to final node of the mesh, nbuf[npre][nr]=node */
/* for cfd more than one node of the mesh can exist based on one node of a certain entity */
int **nbuf=NULL;
int sum_nbuf=0;

/* for CFD-meshing */
int              anz_cfdSurfs=0;
int              writeCFDflag=0;

int   nurbsflag=0, meshopt_length, meshopt_angle, oldmeshflag;
int ebuf[20];
int setall;

int meshPoints( int setNr )
{
  int i,j,meshflag;
  char buffer[MAX_LINE_LENGTH];

  /* schaue ob bodies nachfogend zu meschen sind */
  meshflag=0;
  for (i=0; i<set[setNr].anz_l; i++)
  {
    if(( line[set[setNr].line[i]].etyp==11)||( line[set[setNr].line[i]].etyp==12)) meshflag=1;
  }
  for (i=0; i<set[setNr].anz_s; i++)
  {
    if(( surf[set[setNr].surf[i]].etyp==7)||
       ( surf[set[setNr].surf[i]].etyp==8)||
       ( surf[set[setNr].surf[i]].etyp==9)||
       ( surf[set[setNr].surf[i]].etyp==10)) meshflag=1;
  }
  for (i=0; i<set[setNr].anz_b; i++)
  {
    if(( body[set[setNr].body[i]].etyp==1)||( body[set[setNr].body[i]].etyp==4)) meshflag=1;
  }
  if (fillSurfFlag) meshflag=1;
  if (!meshflag) return(-1);

  for (i=0; i<set[setNr].anz_p; i++)
  {
    j=set[setNr].pnt[i];
    if(printFlag) printf (" meshing point:%s node:%d\n", point[j].name, apre->nmax+1);
    nod( apre, &npre, 0, apre->nmax+1, point[j].px, point[j].py, point[j].pz, 0 );
    if ((point[j].nod = (int *)realloc( (int *)point[j].nod, ((int)1)*sizeof(int)) ) == NULL )
      { printf(" ERROR: realloc failure in meshPoins point:%s can not be meshed\n\n", point[j].name);
        goto noMesh; }
    point[j].nn=1;
    point[j].nod[0]=apre->nmax;
    sprintf( buffer,"%d ", point[j].nod[0] );
  }
  return(1);
  noMesh:;
  return(-1);
}

int straightNodes( int j, int k, int div, double *pn )
/* gives node-positions on a straight line due to line-div */
/* j: line-index, k: nodenr (starts with 0), pn[3]: x,y,z   */
{
  int i;
  static int curk=100, curj=-1, curdiv=-1;
  static double p1[3], p2[3], p1p2[3], ep1p2[3], lp1p2;
  static double l, p1pn[3], *dl=NULL, sum_l;
  /* printf("j:%d k:%d curj:%d %d\n",j,k,curj,curk); */
  if(( j!=curj)||( k<=curk)||(div!=curdiv))
  {
    if( (dl = (double *)realloc( (double *)dl, div*sizeof(double))) == NULL )
    { printf(" ERROR: realloc failure in straightNodes()\n\n"); return(-1); }

    curj=j;
    curdiv=div;
    p1[0]=point[line[j].p1].px;
    p1[1]=point[line[j].p1].py;
    p1[2]=point[line[j].p1].pz;
    p2[0]=point[line[j].p2].px;
    p2[1]=point[line[j].p2].py;
    p2[2]=point[line[j].p2].pz;
    v_result( p1, p2, p1p2 );
    lp1p2=v_betrag( p1p2 );
    v_norm( p1p2, ep1p2 );

    /* berechnung der elementgroessen */
    dl[0]=l=sum_l=1.;
    for(i=1; i<div; i++)
    {
      l= l*line[j].bias; /* aktuelle relative Elementgroesse (1. Element ist 1 lang) */
      sum_l+=l;          /* Summe aller relativen Elementgroessen */ 
      dl[i]= sum_l;
      /* printf("i:%d l:%lf suml:%lf\n", i, l, sum_l); */
    }
    for(i=0; i<div; i++)
    {
      dl[i]= dl[i]/sum_l* lp1p2; /* Summe aller Elementgroessen */
      /* printf("i:%d dl:%lf\n", i,dl[i]);  */
    }
  }
  curk=k;

  /* calculate the node-pos. */
  if (lp1p2==0.)
  {
    pn[0]=p1[0]; pn[1]=p1[1]; pn[2]=p1[2];
  }
  else
  {
    v_scal(&dl[k], ep1p2, p1pn );
    v_add( p1, p1pn, pn );
    /* printf("pn:%f %f %f\n",pn[0],pn[1],pn[2]); */
  }
  return(1);
}

int arcNodes( int j, int k, int div, double *pn )
/* gives node-positions on an arc line due to line-div */
/* j: line-index, k: nodenr (starts with 0), pn[3]: x,y,z   */
{
  int i;
  static int curk=100, curj=-1, curdiv=-1;
  static double p1[3], p2[3], pc[3];
  static double p1p2[3], ep1p2[3], lp1p2;
  static double pcp1[3], lpcp1;
  static double pcp2[3], lpcp2;
  static double pcpn[3], Tp1p2[3], pcT[3], epcT[3];
  static double rad, alfa, beta, *dalfa=NULL, T;
  double rad_cur, l, sum_l;

  /*
  printf("p1: %lf %lf %lf\n", point[line[j].p1].px,point[line[j].p1].py,point[line[j].p1].pz);
  printf("p2: %lf %lf %lf\n", point[line[j].p2].px,point[line[j].p2].py,point[line[j].p2].pz);
  printf("pc: %lf %lf %lf\n", point[line[j].trk].px,point[line[j].trk].py,point[line[j].trk].pz);
  printf("line-index j:%d k:%d div:%d curk:%d curj:%d\n",j,k,div,curk,curj);
  */
  if(( j!=curj)||( k<=curk)||( div!=curdiv))
  {
    if( (dalfa = (double *)realloc( (double *)dalfa, div*sizeof(double))) == NULL )
    { printf(" ERROR: realloc failure in arcNodes()\n\n"); return(-1); }

    curj=j;
    curdiv=div;
    p1[0]=point[line[j].p1].px;
    p1[1]=point[line[j].p1].py;
    p1[2]=point[line[j].p1].pz;
    p2[0]=point[line[j].p2].px;
    p2[1]=point[line[j].p2].py;
    p2[2]=point[line[j].p2].pz;
    pc[0]=point[line[j].trk].px;
    pc[1]=point[line[j].trk].py;
    pc[2]=point[line[j].trk].pz;
  
    v_result( p1, p2, p1p2 );
    v_result( pc, p1, pcp1 );
    v_result( pc, p2, pcp2 );
  
    lp1p2=v_betrag( p1p2 );
    lpcp1=v_betrag( pcp1 );
    lpcp2=v_betrag( pcp2 );
    if ((lp1p2==0.)||(lpcp1==0.)||(lpcp2==0.))
    {
      pn[0]=point[line[j].p1].px;
      pn[1]=point[line[j].p1].py;
      pn[2]=point[line[j].p1].pz;
      return(1);
    }
    rad=(lpcp1+lpcp2)/2.;
    alfa=asin( (lp1p2/2.)/rad ) *2.;
    beta=(PI-alfa)/2.;

    /* berechnung der elementgroessen */
    dalfa[0]=l=sum_l=1.;
    for(i=1; i<div; i++)
    {
      l= l*line[j].bias; /* aktuelle relative Elementgroesse (1. Element ist 1 lang) */
      sum_l+=l;          /* Summe aller relativen Elementgroessen */ 
      dalfa[i]= sum_l;
      // printf("i:%d l:%lf suml:%lf\n", i, l, sum_l); 
    }
    for(i=0; i<div; i++)
    {
      dalfa[i]= dalfa[i]/sum_l * alfa; /* Summe aller Elementgroessen */
      // printf("i:%d dalfa:%lf\n", i,dalfa[i]); 
    }
  }
  curk=k;

  if ((lp1p2==0.)||(lpcp1==0.)||(lpcp2==0.))
  {
    pn[0]=point[line[j].p1].px;
    pn[1]=point[line[j].p1].py;
    pn[2]=point[line[j].p1].pz;
    return(1);
  }

  T=rad/( cos(dalfa[k])/sin(dalfa[k])*sin(beta) + cos(beta) );
  v_norm( p1p2, ep1p2 );
  v_scal(&T, ep1p2, Tp1p2 );

  v_add( pcp1, Tp1p2, pcT );
  v_norm( pcT, epcT );

  rad_cur=(lpcp1*(double)(div-(k+1))/(double)div) + (lpcp2*((double)(k+1)/(double)div));
  v_scal(&rad_cur, epcT, pcpn );

  v_add( pc, pcpn, pn );
  
  // printf("r:%lf r(k/d:%d/%d):%lf a:%lf da:%lf b:%lf T:%lf lp1p2:%lf lpcp1:%lf lpcp2:%lf \n", rad,k,div,rad_cur,alfa,dalfa[k],beta,T,lp1p2,lpcp1,lpcp2);
  
  return(1);
}


int splineNodes( int j, int k, int div, double *pn )
/* gives node-positions on an seq-line (spline) due to line-div */
/* j: line-index, k: nodenr (starts with 0), pn[3]: x,y,z   */
{
  int i, lp[2];
  static int setNr, curdiv=-1, curn=-1, curj=-1, n, mode;
  static double lmax, le, *dl=NULL, sum_l, *l=NULL, *x=NULL, *y=NULL, *z=NULL;
  static double p1[3], p2[3], p1p2[3], lp1p2;
  static double dl1, lsegm;

  setNr=line[j].trk;

  /* determine if we have a new line, new division or number of points */
  if(( j!=curj)||( div!=curdiv)||( set[setNr].anz_p!=curn)||(k==0))
  {
    curj=j;
    curn=set[setNr].anz_p;
    curdiv=div;

    /* realloc the arrays for interpol */
    if( (l = (double *)realloc( (double *)l, curn*sizeof(double))) == NULL )
    { printf(" ERROR: realloc failure in splineNodes()\n\n"); return(-1); }
    if( (dl = (double *)realloc( (double *)dl, div*sizeof(double))) == NULL )
    { printf(" ERROR: realloc failure in splineNodes()\n\n"); return(-1); }
    if( (x = (double *)realloc( (double *)x, curn*sizeof(double))) == NULL )
    { printf(" ERROR: realloc failure in splineNodes()\n\n"); return(-1); }
    if( (y = (double *)realloc( (double *)y, curn*sizeof(double))) == NULL )
    { printf(" ERROR: realloc failure in splineNodes()\n\n"); return(-1); }
    if( (z = (double *)realloc( (double *)z, curn*sizeof(double))) == NULL )
    { printf(" ERROR: realloc failure in splineNodes()\n\n"); return(-1); }

    /* look if the sequence is ordered in the same way as the line */
    lp[0]=lp[1]=0;
    for(i=0; i<set[setNr].anz_p; i++)
    {
      if( line[j].p1==set[setNr].pnt[i] ) lp[0]=i;
      if( line[j].p2==set[setNr].pnt[i] ) lp[1]=i;
    }

    /* determine the length of the spline, accumulate all dists between points */
    /* look if the sequence is ordered in the same way as the line */
    n=1;
    if( lp[0]<lp[1] )
    {
      dl1=lsegm=lmax=l[0]=0.;
      x[0]=p1[0]=point[set[setNr].pnt[lp[0]]].px;
      y[0]=p1[1]=point[set[setNr].pnt[lp[0]]].py;
      z[0]=p1[2]=point[set[setNr].pnt[lp[0]]].pz;
      for (i=lp[0]+1; i<=lp[1]; i++)
      {
        p2[0]=point[set[setNr].pnt[i]].px;
        p2[1]=point[set[setNr].pnt[i]].py;
        p2[2]=point[set[setNr].pnt[i]].pz;
        v_result( p1, p2, p1p2 );
        x[n]=p1[0]=p2[0];
        y[n]=p1[1]=p2[1];
        z[n]=p1[2]=p2[2];
        lp1p2=v_betrag( p1p2 );
        lmax+=lp1p2;              /* total spline length */
        l[n]=lmax;
        if(i==lp[0]) dl1=l[n];    /* spline-length at start of line */
        if((i>lp[0])&&(i<=lp[1]))  lsegm+=lp1p2; /* total line lenght */
        n++;
      }
    }
    else
    {
      dl1=lsegm=lmax=l[0]=0.;
      x[0]=p1[0]=point[set[setNr].pnt[lp[1]]].px;
      y[0]=p1[1]=point[set[setNr].pnt[lp[1]]].py;
      z[0]=p1[2]=point[set[setNr].pnt[lp[1]]].pz;   
      for (i=lp[1]+1; i<=lp[0]; i++)
      {
	// printf("point %d %s\n", i, point[set[setNr].pnt[i]].name);
        p2[0]=point[set[setNr].pnt[i]].px;
        p2[1]=point[set[setNr].pnt[i]].py;
        p2[2]=point[set[setNr].pnt[i]].pz;
        v_result( p1, p2, p1p2 );
        x[n]=p1[0]=p2[0];
        y[n]=p1[1]=p2[1];
        z[n]=p1[2]=p2[2];
        lp1p2=v_betrag( p1p2 );
        lmax+=lp1p2;
        l[n]=lmax;
        if(i==lp[1]) dl1=l[n];    /* spline-length at start of line */
        if((i>lp[1])&&(i<=lp[0]))  lsegm+=lp1p2; /* total line lenght */
        n++;
      }
    }

    /*
    printf(" LINE:%s lmax:%lf\n", line[j].name, lmax);
    for (i=0; i<n; i++)
    {
      printf("%d %lf %lf %lf  %lf %lf %lf %lf \n",i, dl1,lsegm,lmax,l[i],x[i],y[i],z[i]);
    }
    */

    /* berechnung der elementgroessen */
    dl[0]=le=sum_l=1.;
    for(i=1; i<div; i++)
    {
      le= le*line[j].bias; /* aktuelle relative Elementgroesse (1. Element ist 1 lang) */
      sum_l+=le;          /* Summe aller relativen Elementgroessen */ 
      dl[i]= sum_l;
      /* printf("i:%d l:%lf suml:%lf\n", i, l, sum_l); */
    }

    /* scalierung der elemente auf die linienlaenge */
    /* Summe aller Elementgroessen zwischen 1. u i-linienpkt */
    for(i=0; i<div; i++)
    {
      dl[i]= dl1 + dl[i]/sum_l* lsegm; 
      /* printf("i:%d dl:%lf\n", i,dl[i]); */
    }
  }

  /* calculate the node-pos. */
  mode=1;
  pn[0] = intpol3( l, x, n, dl[k], &mode );
  if(mode==-1)
  {
    if(printFlag) printf("WARNING: intpol3 could not create the spline coefficients, intpol2 used\n");
    mode=1;
    pn[0] = intpol2( l, x, n, dl[k], &mode );
  }
  mode=1;
  pn[1] = intpol3( l, y, n, dl[k], &mode );
  if(mode==-1)
  {
    if(printFlag) printf("WARNING: intpol3 could not create the spline coefficients, intpol2 used\n");
    mode=1;
    pn[0] = intpol2( l, y, n, dl[k], &mode );
  }
  mode=1;
  pn[2] = intpol3( l, z, n, dl[k], &mode );
  if(mode==-1)
  {
    if(printFlag) printf("WARNING: intpol3 could not create the spline coefficients, intpol2 used\n");
    mode=1;
    pn[0] = intpol2( l, z, n, dl[k], &mode );
  }
  /*  
  printf ("dl:%lf node[%d] lmax:%lf div:%d pos:%lf %lf %lf\n", dl[k], k, lmax, div, pn[0],pn[1],pn[2]);
  */
  return(1);
}




     /* prototype, preliminary version */

int nurlNodes( int j, int k, int div, double *pn )
/* gives node-positions on a nurbs-line due to line-div */
/* j: line-index, k: nodenr (starts with 0), pn[3]: x,y,z   */
{
  static int curk=100, curj=-1;
  static double p1[3], p2[3], p1p2[3], ep1p2[3], lp1p2;
  static double l, p1pn[3];

  if(( j!=curj)||( k<=curk))
  {
    curj=j;
    p1[0]=point[line[j].p1].px;
    p1[1]=point[line[j].p1].py;
    p1[2]=point[line[j].p1].pz;
    p2[0]=point[line[j].p2].px;
    p2[1]=point[line[j].p2].py;
    p2[2]=point[line[j].p2].pz;
    v_result( p1, p2, p1p2 );
    lp1p2=v_betrag( p1p2 );
    v_norm( p1p2, ep1p2 );
  }
  curk=k;

  /* calculate the node-pos. */
  if (lp1p2==0.)
  {
    pn[0]=p1[0]; pn[1]=p1[1]; pn[2]=p1[2];
  }
  else
  {
    l=lp1p2/div * (k+1.);
    v_scal(&l, ep1p2, p1pn );
    v_add( p1, p1pn, pn );
  }
  return(1);
}


int meshLines( int setNr )
{
  int i,j,k,u,v,meshflag;
  int *nl, noLineMesh=0;
  double pn[3];
  char buffer[MAX_LINE_LENGTH];

  /* schaue ob surfs oder bodies nachfogend zu meschen sind */
  meshflag=0;
  for (i=0; i<set[setNr].anz_s; i++)
  {
    if(( surf[set[setNr].surf[i]].etyp==7)||
       ( surf[set[setNr].surf[i]].etyp==8)||
       ( surf[set[setNr].surf[i]].etyp==9)||
       ( surf[set[setNr].surf[i]].etyp==10)) meshflag=1;
  }
  for (i=0; i<set[setNr].anz_b; i++)
  {
    if(( body[set[setNr].body[i]].etyp==1)||( body[set[setNr].body[i]].etyp==4)) meshflag=1;
  }
  if(fillSurfFlag) meshflag=1;

  for (i=0; i<set[setNr].anz_l; i++)
  {
    j=set[setNr].line[i];
    if ((line[j].etyp==11)||(line[j].etyp==12)||(meshflag))
    {
      line[j].fail=1;
      if(printFlag) printf (" meshing line:%s\n", line[j].name);
    }
    else
      goto noEtypDefined;

    if(line[j].div>0)
      if ((line[j].nod = (int *)realloc( (int *)line[j].nod, (line[j].div)*sizeof(int)) ) == NULL )
      { printf(" ERROR: realloc failure in meshLines Line:%s can not be meshed\n\n", line[j].name);
        goto noMesh; }

    for (k=0; k<line[j].div-1; k++)
    {
      if (line[j].typ=='a')
      {
        if(arcNodes( j, k,line[j].div, pn )==-1)      { noLineMesh++; pre_seta(specialset->nomesh, "l", line[j].name); }
      }
      else if (line[j].typ=='s')
      {
        if(splineNodes( j, k,line[j].div, pn )==-1)   { noLineMesh++; pre_seta(specialset->nomesh, "l", line[j].name); }
      }
      else
      {
        if(straightNodes( j, k,line[j].div, pn )==-1) { noLineMesh++; pre_seta(specialset->nomesh, "l", line[j].name); }
      }
      nod( apre, &npre, 0, apre->nmax+1, pn[0], pn[1], pn[2], 0 );
      line[j].nod[k]=apre->nmax;
      sprintf( buffer,"%d ", line[j].nod[k] );
    }
    line[j].nn=k;
    line[j].fail=0;

    /* correct midside-node-positions if a bias is defined */
    if(line[j].bias!=1.)
    {
      if(line[j].etyp==12) goto corrMidsideNodes;
      for (u=0; u<set[setNr].anz_s; u++)
        if((surf[set[setNr].surf[u]].etyp==8)||(surf[set[setNr].surf[u]].etyp==10)) goto corrMidsideNodes;
      for (u=0; u<set[setNr].anz_b; u++)
        if(body[set[setNr].body[u]].etyp==4) goto corrMidsideNodes;
      goto no_corrMidsideNodes;

      corrMidsideNodes:;
      for (k=-1; k<line[j].nn-1; k+=2)
      {
        if(k==-1)
          if(line[j].nn>1) adjustMidsideNode(&npre[point[line[j].p1].nod[0]].nx,&npre[line[j].nod[k+2]].nx,&npre[line[j].nod[k+1]].nx,0);
          else             adjustMidsideNode(&npre[point[line[j].p1].nod[0]].nx,&npre[point[line[j].p2].nod[0]].nx,&npre[line[j].nod[k+1]].nx,0);
        else if(k==line[j].nn-2) 
          adjustMidsideNode(&npre[line[j].nod[k]].nx,&npre[point[line[j].p2].nod[0]].nx,&npre[line[j].nod[k+1]].nx,0); 
	else 
          adjustMidsideNode(&npre[line[j].nod[k]].nx,&npre[line[j].nod[k+2]].nx,&npre[line[j].nod[k+1]].nx,0);
      }
      
      no_corrMidsideNodes:;
    }

    /* erzeugen der elemente   */
    k=0;

    /* allocate memory for final-node-buffer nbuf and final nodes */
    if ((nbuf = (int **)realloc((int **)nbuf, (apre->nmax+1)*sizeof(int *)) ) == NULL )
    { printf(" ERROR: realloc failure in meshLines, nodes not installed\n\n"); return(-1); }
    for (v=sum_nbuf; v<=apre->nmax; v++)
    {
      if ((nbuf[v] = (int *)malloc( (2)*sizeof(int)) ) == NULL )
      { printf(" ERROR: realloc failure in meshSurf, nodes not installed\n\n"); return(-1); }
      nbuf[v][0]=0;
    }
    sum_nbuf=apre->nmax+1;

    if ((line[j].div>0)&&(!fillSurfFlag))
    {
      if (line[j].etyp==11)
      {
        if ((nl = (int *)malloc( (line[j].div+2)*sizeof(int)) ) == NULL )
        { printf(" ERROR: realloc failure in meshLines Line:%s can not be meshed\n\n", line[j].name);
          goto noMesh; }
        /* elemente duerfen nur allociert werden wenn noetig (NULL-Pointer wird irgendwo abgefragt ) */
        if ((line[j].elem = (int *)realloc( (int *)line[j].elem, (line[j].div)*sizeof(int)) ) == NULL )
        { printf(" ERROR: realloc failure in meshLines Line:%s can not be meshed\n\n", line[j].name);
          goto noMesh; }

        nl[0]=point[line[j].p1].nod[0];
        for (u=1; u<line[j].div; u++)
        {
          nl[u]=line[j].nod[u-1];
        }
        nl[u]=point[line[j].p2].nod[0];

        for (u=0; u<line[j].div; u++)
        {
          elem_define( anz->emax+1, 11, &nl[u], 0, line[j].eattr );
          line[j].elem[k]=anz->emax;
          k++;
        }
        free(nl);
      }

      if (line[j].etyp==12)
      {
        if ((nl = (int *)malloc( (line[j].div+2)*sizeof(int)) ) == NULL )
        { printf(" ERROR: realloc failure in meshLines Line:%s can not be meshed\n\n", line[j].name);
          goto noMesh; }
        /* elemente duerfen nur allociert werden wenn noetig (NULL-Pointer wird irgendwo abgefragt ) */
        if ((line[j].elem = (int *)realloc( (int *)line[j].elem, (line[j].div/2)*sizeof(int)) ) == NULL )
        { printf(" ERROR: realloc failure in meshLines Line:%s can not be meshed\n\n", line[j].name);
          goto noMesh; }

        nl[0]=point[line[j].p1].nod[0];
        for (u=1; u<line[j].div; u++)
        {
          nl[u]=line[j].nod[u-1];
        }
        nl[u]=point[line[j].p2].nod[0];

        for (u=0; u<line[j].div-1; u+=2)
        {
	  ebuf[0]=nl[u];
	  ebuf[1]=nl[u+2];
	  ebuf[2]=nl[u+1];
          elem_define( anz->emax+1, 12, ebuf, 0, line[j].eattr );
          line[j].elem[k]=anz->emax;
          k++;
        }
        free(nl);
      }
    }
    line[j].ne=k;
    
  noEtypDefined:;
  }

  return(noLineMesh);
  noMesh:;
  return(-1);
}


/****************************************************************/
/* get node-numbers of surface-edge-nodes                       */
/* unused areas <n_uv(u,v)> return "-1"                         */
/*                                                              */
/*                                                              */
/*    v                                                         */
/*vmax^                                                         */
/*    |                                                         */
/*    |                                                         */
/*    |                                                         */
/*div_|                                                         */
/*l[1]|                                                         */
/*    |                                                         */
/*    |________________________> u                              */
/* n_uv(0,0) div_l[0]           umax                            */
/****************************************************************/
void edgeNodes( int vmax, int umax, int j, int *n_uv )
{
  int k,l,n,m,o,p, u,v, nodnr;

    for( n=0; n<(umax*vmax); n++) n_uv[n]=-1;

    /* store nodes of edge0 */
    n=0; /* edge Nr */
    u=0; /* surf parameter */
    v=0; /* surf parameter */

    k=surf[j].l[n];
    if( surf[j].typ[n]=='c' )
    {
      if(surf[j].o[n]=='+')
      {
        for( l=0; l<lcmb[k].nl; l++ )
        {
          m=lcmb[k].l[l];
          if(lcmb[k].o[l]=='+')
          {
            if (l==0)
            {
              p=line[m].p1;                    /* Anfangsknoten */
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              u++;
            }
            for( o=0; o<line[m].div-1; o++)  /* alle Zwischenknoten */
            {
              nodnr=line[m].nod[o];
              n_uv[u*vmax +v]=nodnr;
              u++;
       	    }
            p=line[m].p2;                    /* Endknoten */
            nodnr=point[p].nod[0];
            n_uv[u*vmax +v]=nodnr;
            u++;
          }
          else
          {
            if (l==0)
            {
              p=line[m].p2;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              u++;
            }
            for( o=line[m].div-2; o>-1; o--)
            {
              nodnr=line[m].nod[o];
              n_uv[u*vmax +v]=nodnr;
              u++;
            }
              p=line[m].p1;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              u++;
            }
          }
        }
        else  /* edge is - oriented */
        {
          for( l=lcmb[k].nl-1; l>-1; l-- )
          {
            m=lcmb[k].l[l];
            if(lcmb[k].o[l]=='-')
            {
              if (l==lcmb[k].nl-1)
              {
                p=line[m].p1;
                nodnr=point[p].nod[0];
                n_uv[u*vmax +v]=nodnr;
                u++;
              }
              for( o=0; o<line[m].div-1; o++)
              {
                nodnr=line[m].nod[o];
                n_uv[u*vmax +v]=nodnr;
                u++;
              }
              p=line[m].p2;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              u++;
            }
            else
            {
            if (l==lcmb[k].nl-1)
            {
              p=line[m].p2;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              u++;
            }
            for( o=line[m].div-2; o>-1; o--)
            {
              nodnr=line[m].nod[o];
              n_uv[u*vmax +v]=nodnr;
              u++;
            }
            p=line[m].p1;
            nodnr=point[p].nod[0];
            n_uv[u*vmax +v]=nodnr;
            u++;
          }
        }
      }
    }
    else /* its a line */
    {
      m=surf[j].l[n];
      if(surf[j].o[n]=='+')
      {
        p=line[m].p1;
        nodnr=point[p].nod[0];
        n_uv[u*vmax +v]=nodnr;
        u++;
        for( o=0; o<line[m].div-1; o++)
        {
          nodnr=line[m].nod[o];
          n_uv[u*vmax +v]=nodnr;
          u++;
        }
        p=line[m].p2;
        nodnr=point[p].nod[0];
        n_uv[u*vmax +v]=nodnr;
        u++;
      }
      else
      {
        p=line[m].p2;
        nodnr=point[p].nod[0];
        n_uv[u*vmax +v]=nodnr;
        u++;
        for( o=line[m].div-2; o>-1; o--)
        {
          nodnr=line[m].nod[o];
          n_uv[u*vmax +v]=nodnr;
          u++;
        }
        p=line[m].p1;
        nodnr=point[p].nod[0];
        n_uv[u*vmax +v]=nodnr;
        u++;
      }
    }


    /* store nodes of edge1 */
    n=1; /* edge Nr */
    u=umax-1; /* surf parameter */
    v=0;        /* surf parameter */

    k=surf[j].l[n];
    if( surf[j].typ[n]=='c' )
    {
      if(surf[j].o[n]=='+')
      {
        for( l=0; l<lcmb[k].nl; l++ )
        {
          m=lcmb[k].l[l];
          if(lcmb[k].o[l]=='+')
          {
            if (l==0)
            {
              p=line[m].p1;                    /* Anfangsknoten */
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              v++;
            }
            for( o=0; o<line[m].div-1; o++)  /* alle Zwischenknoten */
            {
              nodnr=line[m].nod[o];
              n_uv[u*vmax +v]=nodnr;
              v++;
       	    }
              p=line[m].p2;                    /* Endknoten */
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              v++;
          }
          else
          {
            if (l==0)
            {
              p=line[m].p2;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              v++;
            }
            for( o=line[m].div-2; o>-1; o--)
            {
              nodnr=line[m].nod[o];
              n_uv[u*vmax +v]=nodnr;
              v++;
            }
              p=line[m].p1;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              v++;
	      }
        }
      }
      else  /* edge is - oriented */
      {
        for( l=lcmb[k].nl-1; l>-1; l-- )
        {
          m=lcmb[k].l[l];
          if(lcmb[k].o[l]=='-')
          {
            if (l==lcmb[k].nl-1)
            {
              p=line[m].p1;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              v++;
            }
            for( o=0; o<line[m].div-1; o++)
            {
              nodnr=line[m].nod[o];
              n_uv[u*vmax +v]=nodnr;
              v++;
            }
              p=line[m].p2;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              v++;
          }
          else
          {
            if (l==lcmb[k].nl-1)
            {
              p=line[m].p2;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              v++;
            }
            for( o=line[m].div-2; o>-1; o--)
            {
              nodnr=line[m].nod[o];
              n_uv[u*vmax +v]=nodnr;
              v++;
            }
              p=line[m].p1;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              v++;
          }
        }
      }
    }
    else /* its a line */
    {
      m=surf[j].l[n];
      if(surf[j].o[n]=='+')
      {
        p=line[m].p1;
        nodnr=point[p].nod[0];
          n_uv[u*vmax +v]=nodnr;
          v++;
        for( o=0; o<line[m].div-1; o++)
        {
          nodnr=line[m].nod[o];
          n_uv[u*vmax +v]=nodnr;
          v++;
  	    }
        p=line[m].p2;
        nodnr=point[p].nod[0];
          n_uv[u*vmax +v]=nodnr;
          v++;
      }
      else
      {
        p=line[m].p2;
        nodnr=point[p].nod[0];
          n_uv[u*vmax +v]=nodnr;
          v++;
        for( o=line[m].div-2; o>-1; o--)
        {
          nodnr=line[m].nod[o];
          n_uv[u*vmax +v]=nodnr;
          v++;
        }
        p=line[m].p1;
        nodnr=point[p].nod[0];
          n_uv[u*vmax +v]=nodnr;
          v++;
      }
    }


    /* store nodes of edge2 */
    n=2; /* edge Nr */
    u=umax-1; /* surf parameter */
    v=vmax-1; /* surf parameter */

    k=surf[j].l[n];
    if( surf[j].typ[n]=='c' )
    {
      if(surf[j].o[n]=='+')
      {
        for( l=0; l<lcmb[k].nl; l++ )
        {
          m=lcmb[k].l[l];
          if(lcmb[k].o[l]=='+')
          {
            if (l==0)
            {
              p=line[m].p1;                    /* Anfangsknoten */
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              u--;
            }
            for( o=0; o<line[m].div-1; o++)  /* alle Zwischenknoten */
            {
              nodnr=line[m].nod[o];
              n_uv[u*vmax +v]=nodnr;
              u--;
       	    }
              p=line[m].p2;                    /* Endknoten */
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              u--;
          }
          else
          {
            if (l==0)
            {
              p=line[m].p2;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              u--;
            }
            for( o=line[m].div-2; o>-1; o--)
            {
              nodnr=line[m].nod[o];
              n_uv[u*vmax +v]=nodnr;
              u--;
            }
              p=line[m].p1;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              u--;
	      }
        }
      }
      else  /* edge is - oriented */
      {
        for( l=lcmb[k].nl-1; l>-1; l-- )
        {
          m=lcmb[k].l[l];
          if(lcmb[k].o[l]=='-')
          {
            if (l==lcmb[k].nl-1)
            {
              p=line[m].p1;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              u--;
            }
            for( o=0; o<line[m].div-1; o++)
            {
              nodnr=line[m].nod[o];
              n_uv[u*vmax +v]=nodnr;
              u--;
            }
              p=line[m].p2;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              u--;
          }
          else
          {
            if (l==lcmb[k].nl-1)
            {
              p=line[m].p2;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              u--;
            }
            for( o=line[m].div-2; o>-1; o--)
            {
              nodnr=line[m].nod[o];
              n_uv[u*vmax +v]=nodnr;
              u--;
            }
              p=line[m].p1;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              u--;
          }
        }
      }
    }
    else /* its a line */
    {
      m=surf[j].l[n];
      if(surf[j].o[n]=='+')
      {
        p=line[m].p1;
        nodnr=point[p].nod[0];
          n_uv[u*vmax +v]=nodnr;
          u--;
        for( o=0; o<line[m].div-1; o++)
        {
          nodnr=line[m].nod[o];
          n_uv[u*vmax +v]=nodnr;
          u--;
  	    }
        p=line[m].p2;
        nodnr=point[p].nod[0];
          n_uv[u*vmax +v]=nodnr;
          u--;
      }
      else
      {
        p=line[m].p2;
        nodnr=point[p].nod[0];
          n_uv[u*vmax +v]=nodnr;
          u--;
        for( o=line[m].div-2; o>-1; o--)
        {
          nodnr=line[m].nod[o];
          n_uv[u*vmax +v]=nodnr;
          u--;
        }
        p=line[m].p1;
        nodnr=point[p].nod[0];
          n_uv[u*vmax +v]=nodnr;
          u--;
      }
    }


    /* store nodes of edge3 */
    n=3; /* edge Nr */
    u=0; /* surf parameter */
    v=vmax-1;        /* surf parameter */

    k=surf[j].l[n];
    if( surf[j].typ[n]=='c' )
    {
      if(surf[j].o[n]=='+')
      {
        for( l=0; l<lcmb[k].nl; l++ )
        {
          m=lcmb[k].l[l];
          if(lcmb[k].o[l]=='+')
          {
            if (l==0)
            {
              p=line[m].p1;                    /* Anfangsknoten */
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              v--;
            }
            for( o=0; o<line[m].div-1; o++)  /* alle Zwischenknoten */
            {
              nodnr=line[m].nod[o];
              n_uv[u*vmax +v]=nodnr;
              v--;
       	    }
              p=line[m].p2;                    /* Endknoten */
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              v--;
          }
          else
          {
            if (l==0)
            {
              p=line[m].p2;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              v--;
            }
            for( o=line[m].div-2; o>-1; o--)
            {
              nodnr=line[m].nod[o];
              n_uv[u*vmax +v]=nodnr;
              v--;
            }
              p=line[m].p1;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              v--;
	      }
        }
      }
      else  /* edge is - oriented */
      {
        for( l=lcmb[k].nl-1; l>-1; l-- )
        {
          m=lcmb[k].l[l];
          if(lcmb[k].o[l]=='-')
          {
            if (l==lcmb[k].nl-1)
            {
              p=line[m].p1;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              v--;
            }
            for( o=0; o<line[m].div-1; o++)
            {
              nodnr=line[m].nod[o];
              n_uv[u*vmax +v]=nodnr;
              v--;
            }
              p=line[m].p2;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              v--;
          }
          else
          {
            if (l==lcmb[k].nl-1)
            {
              p=line[m].p2;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              v--;
            }
            for( o=line[m].div-2; o>-1; o--)
            {
              nodnr=line[m].nod[o];
              n_uv[u*vmax +v]=nodnr;
              v--;
            }
              p=line[m].p1;
              nodnr=point[p].nod[0];
              n_uv[u*vmax +v]=nodnr;
              v--;
          }
        }
      }
    }
    else /* its a line */
    {
      m=surf[j].l[n];
      if(surf[j].o[n]=='+')
      {
        p=line[m].p1;
        nodnr=point[p].nod[0];
          n_uv[u*vmax +v]=nodnr;
          v--;
        for( o=0; o<line[m].div-1; o++)
        {
          nodnr=line[m].nod[o];
          n_uv[u*vmax +v]=nodnr;
          v--;
  	    }
        p=line[m].p2;
        nodnr=point[p].nod[0];
          n_uv[u*vmax +v]=nodnr;
          v--;
      }
      else
      {
        p=line[m].p2;
        nodnr=point[p].nod[0];
          n_uv[u*vmax +v]=nodnr;
          v--;
        for( o=line[m].div-2; o>-1; o--)
        {
          nodnr=line[m].nod[o];
          n_uv[u*vmax +v]=nodnr;
          v--;
        }
        p=line[m].p1;
        nodnr=point[p].nod[0];
          n_uv[u*vmax +v]=nodnr;
          v--;
      }
    }
}

/********************************************************/
/* surf has unbalanced edges, check if it is meshable   */
/* and calculate the necessary divisions                */
/*                                                      */
/* the division on edge sb1 will not be changed, all    */
/* other edges might have changed divisions.            */
/*                                                      */
/* in:                                                  */
/* div_l[surfedge]:          original divisions         */
/*                                                      */
/* out:                                                 */
/* div_a:                preliminary div on both side a */
/* div_b:                preliminary div on both side b */
/* sa1, sa2, sb1, sb2:   stores the surf-edges of a, b  */
/*                                                      */
/*  how a and b are determined:                         */
/*    da<=db  da,db are |div_l[]-div_l[]| of opposite   */
/*            edges                                     */
/*   a                                                  */
/*    |                                                 */
/*    |   +^                                            */
/*    |  |_|                                            */
/*    |________ b                                       */
/*                                                      */
/*                                                      */
/*    div_l[sb2]<=div_l[sb1]                            */
/*                                                      */
/* necessary divisions:                                 */
/*  div_a=a1+(db-da)/2                                  */
/*  div_b=b1                                            */
/********************************************************/
int newDivisions( int *div_l, int *div_a,int *div_b, int *sa1,int *sa2,int *sb1,int *sb2 )
{

  int i,div_max,da,db, dd02,dd13, div_a1,div_a2,div_b1,div_b2;
  double fn,dn,n;


  dd02=div_l[0]-div_l[2];
  dd13=div_l[1]-div_l[3];
  if (dd02<0) dd02*=-1;
  if (dd13<0) dd13*=-1;
  if (dd02==dd13)
  {
    div_max=0; for(i=0; i<4; i++) if(div_l[i]>div_max) div_max=div_l[i];
    if((div_l[2]==div_max)||(div_l[0]==div_max))
    {
      da=dd02;
      db=dd13;
      if(div_l[2]<=div_l[0]){ div_a1= div_l[0]+(db-da)/2; div_a2= div_l[2]+(db-da)/2+da; *sa1=0; *sa2=2;}
      else                  { div_a1= div_l[2]+(db-da)/2; div_a2= div_l[0]+(db-da)/2+da; *sa1=2; *sa2=0;}
      if(div_l[3]<=div_l[1]){ div_b1= div_l[1]; div_b2= div_l[3]+db; *sb1=1; *sb2=3;}
      else                  { div_b1= div_l[3]; div_b2= div_l[1]+db; *sb1=3; *sb2=1;}
    }
    else
    {
      db=dd02;
      da=dd13;
      if (div_l[3]<=div_l[1]){div_a1= div_l[1]+(db-da)/2; div_a2= div_l[3]+(db-da)/2+da; *sa1=1; *sa2=3;}
      else                  {div_a1= div_l[3]+(db-da)/2; div_a2= div_l[1]+(db-da)/2+da; *sa1=3; *sa2=1;}
      if (div_l[2]<=div_l[0]){div_b1= div_l[0]; div_b2= div_l[2]+db; *sb1=0; *sb2=2;}
      else                  {div_b1= div_l[2]; div_b2= div_l[0]+db; *sb1=2; *sb2=0;}
    }
  }
  else
  {
    if (dd02<dd13)
    {
      da=dd02;
      db=dd13;
      if(div_l[2]<=div_l[0]){ div_a1= div_l[0]+(db-da)/2; div_a2= div_l[2]+(db-da)/2+da; *sa1=0; *sa2=2;}
      else                  { div_a1= div_l[2]+(db-da)/2; div_a2= div_l[0]+(db-da)/2+da; *sa1=2; *sa2=0;}
      if(div_l[3]<=div_l[1]){ div_b1= div_l[1]; div_b2= div_l[3]+db; *sb1=1; *sb2=3;}
      else                  { div_b1= div_l[3]; div_b2= div_l[1]+db; *sb1=3; *sb2=1;}
    }
    else
    {
      db=dd02;
      da=dd13;
      if (div_l[3]<=div_l[1]){div_a1= div_l[1]+(db-da)/2; div_a2= div_l[3]+(db-da)/2+da; *sa1=1; *sa2=3;}
      else                  {div_a1= div_l[3]+(db-da)/2; div_a2= div_l[1]+(db-da)/2+da; *sa1=3; *sa2=1;}
      if (div_l[2]<=div_l[0]){div_b1= div_l[0]; div_b2= div_l[2]+db; *sb1=0; *sb2=2;}
      else                  {div_b1= div_l[2]; div_b2= div_l[0]+db; *sb1=2; *sb2=0;}
    }
  }
  /* check if (db-da)/2 is positive and integer */
  fn=(double)(db-da)/2.;
  n=(int)fn;
  dn=fn-(double)n;
  if (dn<0.) dn*=-1.;
  if ((fn<0.)||(dn>1e-32))
  {
    printf(" ERROR: in newDivisions(), surf with the div: %d %d %d %d can not be meshed\n",
          div_l[0], div_l[1], div_l[2], div_l[3] );
    printf(" (db-da)/2:%lf dn:%lf\n", fn,dn);
    return(-1);
  }
  else if(printFlag) printf("in newDivisions(): meshable unbalanced edges: %d %d %d %d corrected to a1:%d a2:%d b1:%d b2:%d\n", div_l[0], div_l[1], div_l[2], div_l[3], div_a1, div_a2, div_b1, div_b2 );

  *div_a=div_a1;
  *div_b=div_b1;

  /* determine edge sa1. It is always the line before sb1 to create a propper system */
  n=*sa1;
  if(*sb1==0) *sa1=3; 
  else *sa1=*sb1-1;
  if (*sa1==*sa2) *sa2=n; 

  return(1);
}



/**************************************************************************/
/* nun die Randknoten durch interpol auf die geaenderten                  */
/* divisions umrechnen                                                    */
/* Dann werden die x[u][v], y[u][v], z[u][v] felder gefuellt              */
/* in:                                                                    */
/*  n:       stuetzpunktzahl                                              */
/*  lx,ly,lz Raumkoordinaten der Linienstuetzpunkte                       */
/*  u,v      startwerte                                                   */
/*  flag     flag=0 u laeuft von 0 -> umax-1, v bleibt konstant           */
/*           flag=1 v laeuft von 1 -> vmax-1  u bleibt konstant           */
/*  umax, vmax  groesse des zu fuellenden feldes                          */
/*                                                                        */
/*                                                                        */
/*                                                                        */
/* out:                                                                   */
/*  x[u][v] usw. Raumkoordinaten der Randknoten im surfMesher system      */
/*                                                                        */
/*                                                                        */
/**************************************************************************/
int  newEdgePositions( int n, int u,int v,int umax, int vmax, double *lx,double *ly,double *lz, double *x,double *y,double *z, int flag )
{
  int i, nnew;
  double *ll,fn=0.,dll;
  double *nbez=NULL, *dl=NULL, *dlnew=NULL;


  /* zuerst wird ein feld mit der lauflaenge ll berechnet */
  if ((ll = (double *)malloc( (n)*sizeof(double)) ) == NULL )
  { errMsg(" ERROR: realloc failure in newEdgePositions()\n");
      return(-1); }
  linelength( lx, ly, lz, n, ll );

  if(n<3)
  {
    if (flag) dll=ll[n-1]/(vmax-1);      /* linienlaengen-inkrement */
    else      dll=ll[n-1]/(umax-1);      /* linienlaengen-inkrement */
    x[u*vmax +v] = lx[0];
    y[u*vmax +v] = ly[0];
    z[u*vmax +v] = lz[0];
    fn=dll;
    if (flag)
    {
      for (v=1; v<vmax-1; v++)
      {
        x[u*vmax +v] = intpol( ll, lx, n, fn );
        y[u*vmax +v] = intpol( ll, ly, n, fn );
        z[u*vmax +v] = intpol( ll, lz, n, fn );
      fn+=dll;
      }
    }
    else
    {
      for (u=1; u<umax-1; u++)
      {
        x[u*vmax +v] = intpol( ll, lx, n, fn );
        y[u*vmax +v] = intpol( ll, ly, n, fn );
        z[u*vmax +v] = intpol( ll, lz, n, fn );
        fn+=dll;
      }
    }
    x[u*vmax +v] = lx[n-1];
    y[u*vmax +v] = ly[n-1];
    z[u*vmax +v] = lz[n-1];
    free(ll);

    return(1);
  }
  /* berechne neue lauflaengen-inkremente auf basis der neuen stuetzpunktzahl */
  if (flag) nnew=vmax;
  else      nnew=umax;

  if ((dl = (double *)malloc( (nnew)*sizeof(double)) ) == NULL )
  { errMsg(" ERROR: realloc failure in newEdgePositions()\n");
      return(-1); }
  if ((dlnew = (double *)malloc( (nnew)*sizeof(double)) ) == NULL )
  { errMsg(" ERROR: realloc failure in newEdgePositions()\n");
      return(-1); }
  if ((nbez = (double *)malloc( (nnew)*sizeof(double)) ) == NULL )
  { errMsg(" ERROR: realloc failure in newEdgePositions()\n");
      return(-1); }
  
  for (i=1; i<n; i++) nbez[i-1]= (double)i/(double)(n-1);
  for (i=1; i<n; i++) 
  { 
    dl[i-1] = (ll[i]-ll[i-1])* (n-1)/(nnew-1);
    /* printf(" nbez[%d]:%f dl:%f\n", i-1, nbez[i-1], dl[i-1]); */
  }
  for (i=1; i<nnew; i++)
  {
    dlnew[i-1] = intpol( nbez, dl, n-1, (double)i/(double)(nnew-1));      /* i/(nnew-1) == nbeznew */
    /* fn+=dlnew[i-1]; */
    /* printf(" nbez[%d]:%f dlnew:%f\n", i-1,(double)i/(double)(nnew-1),  dlnew[i-1]); */
  }
  /* for (i=1; i<nnew; i++) dlnew[i-1]*=ll[n-1]/fn; */

  /* neue randkoordinaten */
  x[u*vmax +v] = lx[0];
  y[u*vmax +v] = ly[0];
  z[u*vmax +v] = lz[0];
  fn=dlnew[0];
  if (flag)
  {
    for (v=1; v<nnew-1; v++)
    {
      x[u*vmax +v] = intpol( ll, lx, n, fn  );
      y[u*vmax +v] = intpol( ll, ly, n, fn  );
      z[u*vmax +v] = intpol( ll, lz, n, fn  );
      fn+=dlnew[v];
    }
  }
  else
  {
    for (u=1; u<umax-1; u++)
    {
      x[u*vmax +v] = intpol( ll, lx, n, fn  );
      y[u*vmax +v] = intpol( ll, ly, n, fn  );
      z[u*vmax +v] = intpol( ll, lz, n, fn  );
      fn+=dlnew[u];
    }
  }
  x[u*vmax +v] = lx[n-1];
  y[u*vmax +v] = ly[n-1];
  z[u*vmax +v] = lz[n-1];

  free(ll);
  free(dl);
  free(dlnew);
  free(nbez);
  return(1);
}

/**********************************************************************************/
/* Fuellt ein xyz-feld mit den koordinaten einer Flaeche bei der gegenueber       */
/* liegende seiten ungleiche divisions haben.                                     */
/* Die erforderlichen divisions muessen vorher mit newDivisions() best. werden.   */
/* Die xyz-Koordinaten gelten fuer ein Feld im uv-Raum bei dem die u-achse mit der*/
/* surf[].l[0] zusammenfaellt und die v-achse mit der surf[].l[3].                */
/*                                                                                */
/* in:                                                                            */
/* sur      surface-index                                                         */
/* div_l    feld mit den divisions der surface-edges                              */
/* div_a    ersatzdivision der 'a'-edges der surf.                                */
/* div_b    ersatzdivision der 'b'-edges der surf.                                */
/* sa1,sa2,sb1,sb2  Zuordnung der original-edges zu den a&b-edges                 */
/*                                                                                */
/* out:                                                                           */
/* n_uv        nodeNr im uv-feld, die zusaetzlichen ersatz-stuetzpunkte sind      */
/*             nicht mit nodes belegt da sie bei der Vernetzung rausfallen        */
/* umax, vmax  anzahl der ersatz-stuetzpunkte in 'u' und 'v' bzw entlang edge     */
/*             '0' und '3' ( umax = div_(a|b) +1 )                                */
/* n_ba        nodeNr im ba-feld, siehe n_uv                                      */
/* amax, bmax  anzahl der ersatz-stuetzpunkte in 'a' und 'b'                      */
/*                         ( amax = div_a +1 )                                    */
/* offs_sa1    div_a-div_l[sa1], zur elementerzeugung notwendig                   */
/* offs_sa2    div_a-div_l[sa2]                                                   */
/* x,y,z       f(u,v) aus dem surfmesher, alle u,v positionen sind belegt         */
/*                                                                                */
/*                                                                                */
/**********************************************************************************/
int  fillSurf2( int sur, int *div_l, int div_a, int div_b, int sa1, int sa2, int sb1, int sb2,
 int *n_uv, int *umax, int *vmax, int *n_ba, int *amax, int *bmax, int *offs_sa1, int *offs_sa2,
 double *x, double *y, double *z )
{
  int n,o,m, u,v, b=0,a=0;
  int nodnr;
  double *lx, *ly, *lz; /* line koordinates for linelength() */
  int flag;

  int k;
  double xn, yn, zn;

      *bmax=div_b+1;
      *amax=div_a+1;
      if ((sa1==0)||(sa1==2))
      {
        *umax=*amax    ;
        *vmax=*bmax    ;
      }
      else
      {
        *umax=*bmax    ;
        *vmax=*amax    ;
      }
      edgeNodes( *vmax, *umax, sur, n_uv );

      /* die Randknoten sind nun bekannt. es muessen nun ersatzkoordinaten */
      /* mit dem spacing der ersatzdivissions berechnet werden. dazu wird  */
      /* in einem feld aus koordinate(x,y oder z) und der lauflaenge s int-*/
      /* erpoliert. Ausserdem werden die Randknoten im b,a system          */
      /* gespeichert, dazu ist auch ein offset erforderlich                */

      *offs_sa1=div_a-div_l[sa1];
      *offs_sa2=div_a-div_l[sa2];

      /* side 0 */
      v=0;
      n=div_l[0]+1;        /* anzahl von original-nodes auf der line */
      if (((lx = (double *)malloc( (n)*sizeof(double)) ) == NULL )||
          ((ly = (double *)malloc( (n)*sizeof(double)) ) == NULL )||
          ((lz = (double *)malloc( (n)*sizeof(double)) ) == NULL ))
      { errMsg(" ERROR: realloc failure in fillSurf2()\n");    return(-1); }
      for (u=0; u<n; u++)
      {
        nodnr=n_uv[u* *vmax +v];
        o=0;
        if ((sb1==0)&&(sa1==3)) { b=u                     ; a=0                    ;  o=1 ;}
        if ((sb1==3)&&(sa1==2)) { b=*vmax-1               ; a=u                    ;  o=2 ;}
        if ((sb1==2)&&(sa1==1)) { b=*umax-u-1-*offs_sa2   ; a=*vmax-1              ;  o=3 ;}
        if ((sb1==1)&&(sa1==0)) { b=0                     ; a=*umax-u-1-*offs_sa1  ;  o=4 ;}
        if ((sb1==0)&&(sa1==1)) { b=*umax-u-1             ; a=0                    ;  o=5 ;}
        if ((sb1==3)&&(sa1==0)) { b=0                     ; a=u                    ;  o=6 ;}
        if ((sb1==2)&&(sa1==3)) { b=u+*offs_sa1           ; a=*vmax-1              ;  o=7 ;}
        if ((sb1==1)&&(sa1==2)) { b=*vmax-1               ; a=*umax-u-1-*offs_sa2  ;  o=8 ;}
        n_ba[b* *amax +a]=nodnr;
        lx[u]=npre[nodnr].nx;
        ly[u]=npre[nodnr].ny;
        lz[u]=npre[nodnr].nz;
#if TEST
  printf(" u:%d v:%d umax:%d vmax:%d n:%d x:%lf y:%lf z:%lf \n", u,v,*umax,*vmax, nodnr,
    npre[nodnr].nx,npre[nodnr].ny,npre[nodnr].nz );
  printf(" Fall:%d b:%d a:%d\n", o, b, a);
#endif
      }
      /* nun lx,ly,lz auf die geaenderte division umrechnen */
      u=0;
      flag=0;
      if( newEdgePositions( n, u,v, *umax,*vmax, lx,ly,lz, x,y,z, flag) <0) return(-1);
      free(lx);free(ly);free(lz); 

      /* side 1 */
      u=*umax-1;
      n=div_l[1]+1;
      if (((lx = (double *)malloc( (n)*sizeof(double)) ) == NULL )||
          ((ly = (double *)malloc( (n)*sizeof(double)) ) == NULL )||
          ((lz = (double *)malloc( (n)*sizeof(double)) ) == NULL ))
      { errMsg(" ERROR: realloc failure in fillSurf2()\n");    return(-1); }
      for (v=0; v<n; v++)
      {
        nodnr=n_uv[u**vmax +v];
        o=0;
        if ((sb1==0)&&(sa1==3)) { b=*umax-1              ; a=v                    ;  o=1 ;}
        if ((sb1==3)&&(sa1==2)) { b=*vmax-v-1-*offs_sa2   ; a=*umax-1               ;  o=2 ;}
        if ((sb1==2)&&(sa1==1)) { b=0                   ; a=*vmax-v-1-*offs_sa1    ;  o=3 ;}
        if ((sb1==1)&&(sa1==0)) { b=v                   ; a=0                    ;  o=4 ;}
        if ((sb1==0)&&(sa1==1)) { b=0                   ; a=v                    ;  o=5 ;}
        if ((sb1==3)&&(sa1==0)) { b=v+*offs_sa1          ; a=*umax-1               ;  o=6 ;}
        if ((sb1==2)&&(sa1==3)) { b=*umax-1              ; a=*vmax-v-1-*offs_sa2    ;  o=7 ;}
        if ((sb1==1)&&(sa1==2)) { b=*vmax-v-1            ; a=0                    ;  o=8 ;}
        n_ba[b**amax +a]=nodnr;
        lx[v]=npre[nodnr].nx;
        ly[v]=npre[nodnr].ny;
        lz[v]=npre[nodnr].nz;
      }
      /* nun auf die geaenderte division umrechnen */
      v=0;
      flag=1;
      if( newEdgePositions( n, u,v, *umax,*vmax, lx,ly,lz, x,y,z, flag) <0) return(-1);
      free(lx);free(ly);free(lz); 


      /* side 2 */
      v=*vmax-1;
      n=div_l[2]+1;
      if (((lx = (double *)malloc( (n)*sizeof(double)) ) == NULL )||
          ((ly = (double *)malloc( (n)*sizeof(double)) ) == NULL )||
          ((lz = (double *)malloc( (n)*sizeof(double)) ) == NULL ))
      { errMsg(" ERROR: realloc failure in fillSurf2()\n");    return(-1); }
      m=*umax-n;
      for (u=*umax-1; u>=m; u--)
      {
        nodnr=n_uv[u**vmax +v];
        o=0;
        if ((sb1==0)&&(sa1==3)) { b=u-*offs_sa2          ; a=*vmax-1               ;  o=1 ;}
        if ((sb1==3)&&(sa1==2)) { b=0                   ; a=u-*offs_sa1           ;  o=2 ;}
        if ((sb1==2)&&(sa1==1)) { b=*umax-u-1            ; a=0                    ;  o=3 ;}
        if ((sb1==1)&&(sa1==0)) { b=*vmax-1              ; a=*umax-u-1             ;  o=4 ;}
        if ((sb1==0)&&(sa1==1)) { b=*umax-u-1+*offs_sa1   ; a=*vmax-1               ;  o=5 ;}
        if ((sb1==3)&&(sa1==0)) { b=*vmax-1              ; a=u-*offs_sa2           ;  o=6 ;}
        if ((sb1==2)&&(sa1==3)) { b=u                   ; a=0                    ;  o=7 ;}
        if ((sb1==1)&&(sa1==2)) { b=0                   ; a=*umax-u-1             ;  o=8 ;}
        n_ba[b**amax +a]=nodnr;
        lx[u-m]=npre[nodnr].nx;
        ly[u-m]=npre[nodnr].ny;
        lz[u-m]=npre[nodnr].nz;
      }
      /* nun auf die geaenderte division umrechnen */
      u=0;
      flag=0;
      if( newEdgePositions( n, u,v, *umax,*vmax, lx,ly,lz, x,y,z, flag) <0) return(-1);
      free(lx);free(ly);free(lz); 


      /* side 3 */
      u=0;
      n=div_l[3]+1;
      if (((lx = (double *)malloc( (n)*sizeof(double)) ) == NULL )||
          ((ly = (double *)malloc( (n)*sizeof(double)) ) == NULL )||
          ((lz = (double *)malloc( (n)*sizeof(double)) ) == NULL ))
      { errMsg(" ERROR: realloc failure in fillSurf2()\n");    return(-1); }
      m=*vmax-n;
      for (v=*vmax-1; v>=m; v--)
      {
        nodnr=n_uv[u**vmax +v];
        o=0;
        if ((sb1==0)&&(sa1==3)) { b=0                   ; a=v-*offs_sa1           ;  o=1 ;}
        if ((sb1==3)&&(sa1==2)) { b=*vmax-v-1            ; a=0                    ;  o=2 ;}
        if ((sb1==2)&&(sa1==1)) { b=*umax-1              ; a=*vmax-v-1             ;  o=3 ;}
        if ((sb1==1)&&(sa1==0)) { b=v-*offs_sa2          ; a=*umax-1               ;  o=4 ;}
        if ((sb1==0)&&(sa1==1)) { b=*umax-1              ; a=v-*offs_sa2           ;  o=5 ;}
        if ((sb1==3)&&(sa1==0)) { b=v                   ; a=0                    ;  o=6 ;}
        if ((sb1==2)&&(sa1==3)) { b=0                   ; a=*vmax-v-1             ;  o=7 ;}
        if ((sb1==1)&&(sa1==2)) { b=*vmax-v-1+*offs_sa1   ; a=*umax-1               ;  o=8 ;}
        n_ba[b**amax +a]=nodnr;
        lx[v-m]=npre[nodnr].nx;
        ly[v-m]=npre[nodnr].ny;
        lz[v-m]=npre[nodnr].nz;
      }
      /* nun auf die geaenderte division umrechnen */
      v=0;
      flag=1;
      if( newEdgePositions( n, u,v, *umax,*vmax, lx,ly,lz, x,y,z, flag) <0) return(-1);
      free(lx);free(ly);free(lz); 


      /* auffuellen der surface mit temporaeren-nodes */
      /* die vernetzung (nodes, elem generierung) erfolgt  */
      /* im ba system. Jedoch werden sie zur verwendung in */
      /* meshBodies() im uv system in der surf abgelegt     */
      k=0;
      surfMesh( vmax, umax, x, y, z);
      for (u=1; u< *umax-1; u++)
      {
        for (v=1; v< *vmax-1; v++)
        {
          xn=x[u* *vmax +v];
          yn=y[u* *vmax +v];
          zn=z[u* *vmax +v];
          nod(  apre, &npre, 0, apre->nmax+1, xn, yn, zn, 0 );

          /* apre->nmax wird in nod um 1 erhoeht!  */
          n_uv[u* *vmax +v]=apre->nmax;

          /* umspeichern der temp-nodes ins ba system */
          o=0;
          if ((sb1==0)&&(sa1==3)) { b=u                   ; a=v                    ;  o=1 ;}
          if ((sb1==3)&&(sa1==2)) { b=*vmax-v-1           ; a=u                    ;  o=2 ;}
          if ((sb1==2)&&(sa1==1)) { b=*umax-u-1           ; a=*vmax-v-1            ;  o=3 ;}
          if ((sb1==1)&&(sa1==0)) { b=v                   ; a=*umax-u-1            ;  o=4 ;}
          if ((sb1==0)&&(sa1==1)) { b=*umax-u-1           ; a=v                    ;  o=5 ;}
          if ((sb1==3)&&(sa1==0)) { b=v                   ; a=u                    ;  o=6 ;}
          if ((sb1==2)&&(sa1==3)) { b=u                   ; a=*vmax-v-1            ;  o=7 ;}
          if ((sb1==1)&&(sa1==2)) { b=*vmax-v-1           ; a=*umax-u-1            ;  o=8 ;}
          n_ba[b* *amax +a]=apre->nmax;
#if TEST
  printf(" Fall:%d b:%d a:%d\n", o, b, a);
#endif
          surf[sur].nod[k]=apre->nmax;
          sprintf( buffer,"%d ", surf[sur].nod[k] );
          k++;
        }
      }
      surf[sur].nn=k;

      /* randbereiche umspeichern (nodenr der diagonale den raendern der undefinierten luecken zuordnen) */
      a=div_l[sa1];
      for (b=1; b<*offs_sa1; b++)
      {
        n_ba[b* *amax +a]=n_ba[b* *amax +(div_a-b)];
#if TEST
  printf("1 node:%d b:%d a:%d (div_a-b):%d\n", n_ba[b* *amax +a], b, a,(div_a-b));
#endif
      }
      o=0;
      b=*offs_sa1;
      for (a=div_l[sa1]+1; a<div_a; a++)
      {
        o++;
        n_ba[b* *amax +a]=n_ba[(b-o)* *amax +a];
#if TEST
  printf("2 node:%d b:%d a:%d\n", n_ba[b* *amax +a], b, a);
#endif
      }
      o=0;
      a=div_l[sa2];
      for (b=div_b-1; b>div_b - *offs_sa2; b--)
      {
        o++;
        n_ba[b* *amax +a]=n_ba[b* *amax +(div_a-o)];
#if TEST
  printf("3 node:%d b:%d a:%d\n", n_ba[b* *amax +a], b, a);
#endif
     }
      o=0;
      b=div_b- *offs_sa2;
      for (a=div_l[sa2]+1; a<div_a; a++)
      {
        o++;
        n_ba[b* *amax +a]=n_ba[(b+o)* *amax +a];
#if TEST
  printf("4 node:%d b:%d a:%d\n", n_ba[b* *amax +a], b, a);
#endif
      }
  return(1);
}


/**********************************************************************************/
/* Fuellt ein xyz-feld mit den koordinaten einer Flaeche bei der gegenueber       */
/* liegende seiten gleiche divisions haben.                                       */
/* Die xyz-Koordinaten gelten fuer ein Feld im uv-Raum bei dem die u-achse mit der*/
/* surf[].l[0] zusammenfaellt und die v-achse mit der surf[].l[3].                */
/*                                                                                */
/*                                                                                */
/*                                                                                */
/* in:                                                                            */
/* j           surface-index                                                      */
/* umax, vmax  anzahl der ersatz-stuetzpunkte in 'u' und 'v' bzw entlang edge     */
/*             '0' und '3'                                                        */
/*                                                                                */
/* out:                                                                           */
/* n_uv        nodeNr im uv-feld                                                  */
/* x,y,z       f(u,v) aus dem surfmesher, alle u,v positionen sind belegt         */
/*                                                                                */
/*                                                                                */
/**********************************************************************************/
int  fillSurf( int j, int *n_uv, int umax, int vmax, double *x, double *y, double *z )
{
  int u, v, nodnr, k;
  double xn, yn, zn;

      edgeNodes( vmax, umax, j, n_uv );
      /* auffuellen der Surface-randfelder */
      for (u=0; u<umax; u++)
      {
        for (v=0; v<vmax; v++)
        {
          nodnr=n_uv[u*vmax +v];
          if (nodnr>-1)
          {
            x[u*vmax +v]=npre[nodnr].nx;
            y[u*vmax +v]=npre[nodnr].ny;
            z[u*vmax +v]=npre[nodnr].nz;
          }	  
#if TEST
 printf(" u:%d v:%d umax:%d vmax:%d n:%d x:%lf y:%lf z:%lf x:%lf y:%lf z:%lf\n", u,v,umax,vmax, nodnr,
   x[u*vmax +v], y[u*vmax +v],z[u*vmax +v],npre[nodnr].nx,npre[nodnr].ny,npre[nodnr].nz );
#endif	  
        }
      }

      /* auffuellen der surface mit nodes */
      k=0;
      surfMesh( &vmax, &umax, x, y, z);
      for (u=1; u<umax-1; u++)
      {
        for (v=1; v<vmax-1; v++)
        {
          xn=x[u*vmax +v];
          yn=y[u*vmax +v];
          zn=z[u*vmax +v];
          nod( apre, &npre, 0, apre->nmax+1, xn, yn, zn, 0 );
          /* apre->nmax wird in nod um 1 erhoeht!  */
          n_uv[u*vmax +v]=apre->nmax;
          surf[j].nod[k]=apre->nmax;
          sprintf( buffer,"%d ", surf[j].nod[k] );
          k++;
        }
      }
      surf[j].nn=k;
  return(1);
}



/*******************************************************************************************/
/* splits line or lcmb at a certain position and returns 2 lines with nodes and elements   */
/* of the original one                                                                     */
/* in:                                                                                     */
/* edge      line or lcmb index                                                            */
/* typ       l for line or c for lcmb                                                      */
/* splitdiv  splitting location in terms of node-divisions                                 */
/*           for example if the combined division of the lcmb is 10 then a value of .5     */
/*           would split the lcmb at node-position 6 and two lcmbs with div 5 are returned */
/* out:                                                                                    */
/* lnew      2 lines or lcmbs                                                              */
/* typnew    l for line or c for lcmb                                                      */
/* returns -1 if failed or split-point-index if successfull                                                */
/*******************************************************************************************/

int splitLineAtDivratio(int edge, int typ, double splitdiv, int *lnew, char  *typnew)
{
  int c=0, i, j, l=0, n;
  int sum_div, div=0;
  int ps, cl=0, cnew[2];
  int seq[2], setNr; 
  double v[3], p0[3], p1[3], p01[3], lbez, ps_lbez;
  char name[MAX_LINE_LENGTH];
  static int *lin=NULL;
  static char *ori;

  static double *pset_dl=NULL;


  /* define the split-point location and the line to split */
  if( typ=='l')
  {
    l= edge;
    div=line[l].div*splitdiv-1;
    if(div<0) return(-1);
  }
  else if( typ=='c')
  {
    if ((ori = (char *)realloc( (char *)ori, (lcmb[c].nl)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: realloc failure in splitLineAtDivratio\n\n"); return(-1); }
    if ((lin = (int *)realloc( (int *)lin, (lcmb[c].nl)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in splitLineAtDivratio\n\n"); return(-1); }

    /* search the midspan line-index */ 
    c= edge;
    sum_div=div=0;
    for(i=0; i<lcmb[c].nl; i++)
    {
      l=lcmb[c].l[i];
      sum_div+=line[l].div;
    }
    for(i=0; i<lcmb[c].nl; i++)
    {
      l=lcmb[c].l[i];
      div+=line[l].div;
      if(div>sum_div*splitdiv) break;
    }
    cl=i;
    if(lcmb[c].o[cl]=='+') div=line[l].div-(div-sum_div*splitdiv)-1;
    else                  div=(div-sum_div*splitdiv)-1;
  }

  /* check if the split-point would be at one of the ends of the line or if one line must be splitted */
  if(div==-1)
  {
    /* 1st Line-Point is the splitting point, no line must be splitted */
    ps=line[l].p1;

    /* create two lcmb */
    if(lcmb[c].o[cl]=='+')
    {
      if(cl==0) return(-1);
      else if(cl==1)
      { 
        lnew[0]=lcmb[c].l[0]; 
        typnew[0]='l';
      }
      else
      { 
        for(i=0; i<cl; i++)
        {
          lin[i]=lcmb[c].l[i];
          ori[i]=lcmb[c].o[i];
        }
        n= getNewName( name, "c" );
        lnew[0]= lcmb_i( name, 0, i, ori, lin ); typnew[0]='c';
        pre_seta(specialset->zap, "c", name);
      }

      if(cl==(lcmb[c].nl-1))
      {
        lnew[1]=lcmb[c].l[cl];
        typnew[1]='l';
      }
      else
      { 
        for(i=cl; i<lcmb[c].nl; i++)
        {
          lin[i-cl]=lcmb[c].l[i];
          ori[i-cl]=lcmb[c].o[i];
        }
        n= getNewName( name, "c" );
        lnew[1]= lcmb_i( name, 0, i-cl, ori, lin ); typnew[1]='c';
        pre_seta(specialset->zap, "c", name);
      }

    }
    else 
    {

      if(cl==0) { lnew[0]=lcmb[c].l[0]; typnew[0]='l'; }
      else
      { 
        for(i=0; i<=cl; i++)
        {
          lin[i]=lcmb[c].l[i];
          ori[i]=lcmb[c].o[i];
        }
        n= getNewName( name, "c" );
        lnew[0]= lcmb_i( name, 0, i, ori, lin ); typnew[0]='c';
        pre_seta(specialset->zap, "c", name);
      }

      if(cl==(lcmb[c].nl-1)) { lnew[1]=lcmb[c].l[cl]; typnew[1]='l'; }
      else
      { 
        for(i=cl+1; i<lcmb[c].nl; i++)
        { 
          lin[i-cl-1]=lcmb[c].l[i];
          ori[i-cl-1]=lcmb[c].o[i];
        }
        n= getNewName( name, "c" );
        lnew[1]= lcmb_i( name, 0, i-cl-1, ori, lin ); typnew[1]='c';
        pre_seta(specialset->zap, "c", name);
      }

    }
  }
  else if(div==line[l].div-1)
  {
    /* last Line-Point is the splitting point, no line must be splitted */
    ps=line[l].p2;

    /* create two lcmb */
    if(lcmb[c].o[cl]=='-')
    {
      if(cl==0) return(-1);
      else if(cl==1)
      { 
        lnew[0]=lcmb[c].l[0]; 
        typnew[0]='l';
      }
      else
      { 
        for(i=0; i<cl; i++)
        {
          lin[i]=lcmb[c].l[i];
          ori[i]=lcmb[c].o[i];
        }
        n= getNewName( name, "c" );
        lnew[0]= lcmb_i( name, 0, i, ori, lin ); typnew[0]='c';
        pre_seta(specialset->zap, "c", name);
      }

      if(cl==(lcmb[c].nl-1))
      {
        lnew[1]=lcmb[c].l[cl];
        typnew[1]='l';
      }
      else
      { 
        for(i=cl; i<lcmb[c].nl; i++)
        {
          lin[i-cl]=lcmb[c].l[i];
          ori[i-cl]=lcmb[c].o[i];
        }
        n= getNewName( name, "c" );
        lnew[1]= lcmb_i( name, 0, i-cl, ori, lin ); typnew[1]='c';
        pre_seta(specialset->zap, "c", name);
      }

    }
    else 
    {

      if(cl==0) { lnew[0]=lcmb[c].l[0]; typnew[0]='l'; }
      else
      { 
        for(i=0; i<=cl; i++)
        {
          lin[i]=lcmb[c].l[i];
          ori[i]=lcmb[c].o[i];
        }
        n= getNewName( name, "c" );
        lnew[0]= lcmb_i( name, 0, i, ori, lin ); typnew[0]='c';
        pre_seta(specialset->zap, "c", name);
      }

      if(cl==(lcmb[c].nl-1)) { lnew[1]=lcmb[c].l[cl]; typnew[1]='l'; }
      else
      { 
        for(i=cl+1; i<lcmb[c].nl; i++)
        { 
          lin[i-cl-1]=lcmb[c].l[i];
          ori[i-cl-1]=lcmb[c].o[i];
        }
        n= getNewName( name, "c" );
        lnew[1]= lcmb_i( name, 0, i-cl-1, ori, lin ); typnew[1]='c';
        pre_seta(specialset->zap, "c", name);
      }

    }
    
  }
  else
  {
    /* create a splitting point */
    /* and two lines or lcmb */

    if (line[l].typ=='a')
    {
      arcNodes( l, div, line[l].div, v );
    }
    else if (line[l].typ=='s')
    {
      splineNodes( l, div, line[l].div, v );
    }
    else
    {
      straightNodes( l, div, line[l].div, v );
    }
    n= getNewName( name, "p" );
    ps= pnt( name, v[0], v[1], v[2], 0 );
    pre_seta(specialset->zap, "p", name);


    /* create two lines/lcmbs out of the original one and apply the nodes/elements of the original */

    /* create 2 new lines */ 
    if (line[l].typ=='a')
    {
      n= getNewName( name, "l" );
      lnew[0]= line_i( name, line[l].p1, ps, line[l].trk, div+1, 1, line[l].typ );
      if(lnew[0]<0) { printf("ERROR: line could not be created\n"); return(-1); }
      pre_seta(specialset->zap, "l", name);
      n= getNewName( name, "l" );
      lnew[1]= line_i( name, ps, line[l].p2, line[l].trk, line[l].div-(div+1), 1, line[l].typ );
      if(lnew[1]<0) { printf("ERROR: line could not be created\n"); return(-1); }
      pre_seta(specialset->zap, "l", name);
    }
    else if (line[l].typ=='s')
    {
      /* erzeuge zwei neue sets fuer die stuetzpunkte der zwei neuen linien */
      n= getNewName( name, "se" );
      seq[0]=pre_seta( name, "is", 0);
      if ( seq[0] <0 )
      { printf(" ERROR in splitLine\n"); return(-1); }
      pre_seta(specialset->zap, "r", name);

      n= getNewName( name, "se" );
      seq[1]=pre_seta( name, "is", 0);
      if ( seq[1] <0 )
      { printf(" ERROR in splitLine\n"); return(-1); }
      pre_seta(specialset->zap, "r", name);

      /* determine the length of the spline, accumulate all dists between points */
      /* look if the sequence is ordered in the same way as the line */
      setNr=line[l].trk;
      lbez=0.;
      if ( ( pset_dl= (double *)realloc( (double *)pset_dl, (set[setNr].anz_p+1) * sizeof(double))) == NULL )
        printf("ERROR: realloc failed: isort\n\n" ); 
      pset_dl[0]=0.;
      if( line[l].p1==set[setNr].pnt[0] )
      {
        p0[0]=point[set[setNr].pnt[0]].px;
        p0[1]=point[set[setNr].pnt[0]].py;
        p0[2]=point[set[setNr].pnt[0]].pz;   
        for (i=1; i<set[setNr].anz_p; i++)
        {
          p1[0]=point[set[setNr].pnt[i]].px;
          p1[1]=point[set[setNr].pnt[i]].py;
          p1[2]=point[set[setNr].pnt[i]].pz;
          v_result( p0, p1, p01  );
          p0[0]=p1[0];
          p0[1]=p1[1];
          p0[2]=p1[2];
          lbez+=v_betrag( p01  );
          pset_dl[i]=lbez;
        }
      }
      else
      {
        p0[0]=point[set[setNr].pnt[set[setNr].anz_p-1]].px;
        p0[1]=point[set[setNr].pnt[set[setNr].anz_p-1]].py;
        p0[2]=point[set[setNr].pnt[set[setNr].anz_p-1]].pz;   
        for (i=1; i<set[setNr].anz_p; i++)
        {
          p1[0]=point[set[setNr].pnt[set[setNr].anz_p-1-i]].px;
          p1[1]=point[set[setNr].pnt[set[setNr].anz_p-1-i]].py;
          p1[2]=point[set[setNr].pnt[set[setNr].anz_p-1-i]].pz;
          v_result( p0, p1, p01  );
          p0[0]=p1[0];
          p0[1]=p1[1];
          p0[2]=p1[2];
          lbez+=v_betrag( p01  );
          pset_dl[set[setNr].anz_p-1-i]=lbez;
        }
      }
      ps_lbez=lbez*splitdiv;

      n=0;
      for (i=0; i<set[setNr].anz_p; i++)
      { 
        if(pset_dl[i]<ps_lbez) seta( seq[0], "p", set[setNr].pnt[i] );
        else
        {
          if(!n){
            seta( seq[0], "p", ps ); 
            seta( seq[1], "p", ps ); n++; } 
          seta( seq[1], "p", set[setNr].pnt[i] );
        }
      }

      n= getNewName( name, "l" );
      lnew[0]= line_i( name, line[l].p1, ps, seq[0], div+1, 1, line[l].typ );
      if(lnew[0]<0) { printf("ERROR: line could not be created\n"); return(-1); }
      pre_seta(specialset->zap, "l", name);
      n= getNewName( name, "l" );
      lnew[1]= line_i( name, ps, line[l].p2, seq[1], line[l].div-(div+1), 1, line[l].typ );
      if(lnew[1]<0) { printf("ERROR: line could not be created\n"); return(-1); }
      pre_seta(specialset->zap, "l", name);
    }
    else
    {
      n= getNewName( name, "l" );
      lnew[0]= line_i( name, line[l].p1, ps, -1, div+1, 1, line[l].typ );
      if(lnew[0]<0) { printf("ERROR: line could not be created\n"); return(-1); }
      pre_seta(specialset->zap, "l", name);
      n= getNewName( name, "l" );
      lnew[1]= line_i( name, ps, line[l].p2, -1, line[l].div-(div+1), 1, line[l].typ );
      if(lnew[1]<0) { printf("ERROR: line could not be created\n"); return(-1); }
      pre_seta(specialset->zap, "l", name);
    }
    typnew[0]=typnew[1]='l';

    /* map the nodes of the original line and additional point to the two new ones */
    if ((point[ps].nod = (int *)realloc( (int *)point[ps].nod, ((int)1)*sizeof(int)) ) == NULL )
      { printf(" ERROR: realloc failure in splitLineAtDivratio, point:%s can not be meshed\n\n", point[ps].name); return(-1); }
    point[ps].nod[0]=line[l].nod[div];

    if(div>0)
      if ((line[lnew[0]].nod = (int *)realloc( (int *)line[lnew[0]].nod, (div)*sizeof(int)) ) == NULL )
      { printf(" ERROR: realloc failure in meshLines Line:%s can not be meshed\n\n", line[lnew[0]].name); return(-1); }
    for (i=0; i<div; i++)
    {
      line[lnew[0]].nod[i]=line[l].nod[i];
    }
    line[lnew[0]].nn=i;

    if(line[l].div-2-div>0)
      if ((line[lnew[1]].nod = (int *)realloc( (int *)line[lnew[1]].nod, (line[l].div-2-div)*sizeof(int)) ) == NULL )
      { printf(" ERROR: realloc failure in meshLines Line:%s can not be meshed\n\n", line[lnew[0]].name); return(-1); }
    j=0;
    for (i=div+1; i<line[l].div-1; i++)
    {
      line[lnew[1]].nod[j]=line[l].nod[i];
      j++;
    }
    line[lnew[1]].nn=j;


    /* change lnew if the edge is an lcmb */ 
    if( typ=='c')
    {
      if(lcmb[c].o[cl]=='-')
      {
        cnew[0]=lnew[1];
        cnew[1]=lnew[0];
        lnew[0]=cnew[0];
        lnew[1]=cnew[1];
      }
      if(cl>0)
      { 
        for(i=0; i<cl; i++)
        {
          lin[i]=lcmb[c].l[i];
          ori[i]=lcmb[c].o[i];
        }
        lin[i]=lnew[0];
        ori[i]=lcmb[c].o[cl];
        n= getNewName( name, "c" );
        lnew[0]= lcmb_i( name, 0, i+1, ori, lin ); typnew[0]='c';
        pre_seta(specialset->zap, "c", name);
      }

      if(cl<lcmb[c].nl-1)
      { 
        lin[0]=lnew[1];
        ori[0]=lcmb[c].o[cl];
        j=1;
        for(i=cl+1; i<lcmb[c].nl; i++)
        {
          lin[j]=lcmb[c].l[i];
          ori[j]=lcmb[c].o[i];
          j++;
        }
        n= getNewName( name, "c" );
        lnew[1]= lcmb_i( name, 0, j, ori, lin ); typnew[1]='c';
        pre_seta(specialset->zap, "c", name);
      }
    }
 
  }
  return(ps);
}


/* creates a lcmb out of two lines or lcmbs */
/* returns index of an lcmb or -1 if failed */
int addTwoLines( int l1, char o1, char typ1, int l2, char o2, char typ2 )
{
  int i, j, n, c, *lin;
  char name[MAX_LINE_LENGTH], *ori;

  if((typ1=='l')&&(typ2=='l'))
  {
    if ((ori = (char *)malloc((2)*sizeof(char)) ) == NULL )
    { printf("\n\nERROR: realloc failure in splitLineAtDivratio\n\n"); return(-1); }
    if ((lin = (int *)malloc((2)*sizeof(int)) ) == NULL )
    { printf("\n\nERROR: realloc failure in splitLineAtDivratio\n\n"); return(-1); }
    lin[0]=l1;
    ori[0]=o1;
    lin[1]=l2;
    ori[1]=o2;
    n= getNewName( name, "c" );
    c = lcmb_i( name, 0, 2, ori, lin );
    pre_seta(specialset->zap, "c", name);
    free(ori); free(lin);
  }
  else if((typ1=='l')&&(typ2=='c'))
  {
    if ((ori = (char *)malloc((lcmb[l2].nl+1)*sizeof(char)) ) == NULL )
    { printf("\n\nERROR: realloc failure in splitLineAtDivratio\n\n"); return(-1); }
    if ((lin = (int *)malloc((lcmb[l2].nl+1)*sizeof(int)) ) == NULL )
    { printf("\n\nERROR: realloc failure in splitLineAtDivratio\n\n"); return(-1); }
    lin[0]=l1;
    ori[0]=o1;
    j=1;
    if(o2=='+')
    {
      for( i=0; i<lcmb[l2].nl; i++)
      {
        lin[j]=lcmb[l2].l[i];
        ori[j]=lcmb[l2].o[i];
        j++;
      }
    }
    else
    {
      for( i=lcmb[l2].nl-1; i>=0; i--)
      {
        lin[j]=lcmb[l2].l[i];
        ori[j]=lcmb[l2].o[i];
        j++;
      }
    }
    n= getNewName( name, "c" );
    c = lcmb_i( name, 0, j, ori, lin );
    pre_seta(specialset->zap, "c", name);
    free(ori); free(lin);
  }
  else if((typ1=='c')&&(typ2=='l'))
  {
    if ((ori = (char *)malloc((lcmb[l1].nl+1)*sizeof(char)) ) == NULL )
    { printf("\n\nERROR: realloc failure in splitLineAtDivratio\n\n"); return(-1); }
    if ((lin = (int *)malloc((lcmb[l1].nl+1)*sizeof(int)) ) == NULL )
    { printf("\n\nERROR: realloc failure in splitLineAtDivratio\n\n"); return(-1); }
    j=0;
    if(o1=='+')
    {
      for( i=0; i<lcmb[l1].nl; i++)
      {
        lin[j]=lcmb[l1].l[i];
        ori[j]=lcmb[l1].o[i];
        j++;
      }
    }
    else
    {
      for( i=lcmb[l1].nl-1; i>=0; i--)
      {
        lin[j]=lcmb[l1].l[i];
        ori[j]=lcmb[l1].o[i];
        j++;
      }
    }
    lin[j]=l2;
    ori[j]=o2;
    j++;
    n= getNewName( name, "c" );
    c = lcmb_i( name, 0, j, ori, lin );
    pre_seta(specialset->zap, "c", name);
    free(ori); free(lin);
  }
  else if((typ1=='c')&&(typ2=='c'))
  {
    if ((ori = (char *)malloc((lcmb[l1].nl+lcmb[l2].nl)*sizeof(char)) ) == NULL )
    { printf("\n\nERROR: realloc failure in splitLineAtDivratio\n\n"); return(-1); }
    if ((lin = (int *)malloc((lcmb[l1].nl+lcmb[l2].nl)*sizeof(int)) ) == NULL )
    { printf("\n\nERROR: realloc failure in splitLineAtDivratio\n\n"); return(-1); }
    j=0;
    if(o1=='+')
    {
      for( i=0; i<lcmb[l1].nl; i++)
      {
        lin[j]=lcmb[l1].l[i];
        ori[j]=lcmb[l1].o[i];
        j++;
      }
    }
    else
    {
      for( i=lcmb[l1].nl-1; i>=0; i--)
      {
        lin[j]=lcmb[l1].l[i];
        ori[j]=lcmb[l1].o[i];
        j++;
      }
    }
    if(o2=='+')
    {
      for( i=0; i<lcmb[l2].nl; i++)
      {
        lin[j]=lcmb[l2].l[i];
        ori[j]=lcmb[l2].o[i];
        j++;
      }
    }
    else
    {
      for( i=lcmb[l2].nl-1; i>=0; i--)
      {
        lin[j]=lcmb[l2].l[i];
        ori[j]=lcmb[l2].o[i];
        j++;
      }
    }
    n= getNewName( name, "c" );
    c = lcmb_i( name, 0, j, ori, lin );
    pre_seta(specialset->zap, "c", name);
    free(ori); free(lin);
  }
  else return(-1);
  return(c); 
}

int determineBestCorners( int s, int *cl)
{
  int i,j;
  double v01[3],v02[3],vnorm[3];
  double *fi,  fi_min=MAX_INTEGER;

  if((fi=(double *)malloc((surf[s].nl+1)*sizeof(double) ) )==NULL) 
  { printf(" ERROR: malloc failure in determineBestCorners()\n"); return(-1); }

  /* calculate the angle between two corners based on the end-points */
  for(i=0; i<surf[s].nl; i++)
  {
    if(i==surf[s].nl-1) j=0;
    else j=i+1;
    if(surf[s].typ[i]=='l')
    {
      v_result( &point[line[surf[s].l[i]].p1].px,&point[line[surf[s].l[i]].p2].px , v01 );
    }
    else
    {
      v_result( &point[lcmb[surf[s].l[i]].p1].px,&point[lcmb[surf[s].l[i]].p2].px , v01 );
    }
    if(surf[s].typ[j]=='l')
    {
      v_result( &point[line[surf[s].l[j]].p1].px,&point[line[surf[s].l[j]].p2].px , v02 );
    }
    else
    {
      v_result( &point[lcmb[surf[s].l[j]].p1].px,&point[lcmb[surf[s].l[j]].p2].px , v02 );
    }

    /* determine the smallest angle between two lines */
    /*
    v_norm( v01, e01 ); 
    v_norm( v02, e02 ); 
    fi[i]=abs(v_sprod( e01, e02 ));
    if(fi[i]>fi_max) { fi_max=fi[i]; cl[0]=i; cl[1]=j; }
    */

    /* determine the smallest cross-product between two lines (small angle and small lines) */
    v_prod( v01, v02, vnorm);
    fi[i]=abs(v_betrag(vnorm));
    if(fi[i]<fi_min) { fi_min=fi[i]; cl[0]=i; cl[1]=j; }

#if TEST
    if(surf[s].typ[i]=='l') printf("fi:%lf %s\n", fi[i], line[surf[s].l[i]].name);
            else  printf("fi:%lf %s\n", fi[i], lcmb[surf[s].l[i]].name);
    if(surf[s].typ[j]=='l') printf("     %s\n", line[surf[s].l[j]].name);
            else  printf("     %s\n", lcmb[surf[s].l[j]].name);
#endif
  }

  free(fi);
  return(1);  
}

int mesh_tr3u(int nr )
{
  int i,j,l, oprod;
  int m,n;    
  int k;  
  int p;    

  extern Meshp meshp;
  double p0[3], p0p1[3], p1[3];
  int   patch;

  int     e=0, np, *npc=NULL, nurbsnr; 
  double *pnt_u=NULL, *pnt_v=NULL;
  int    *tri=NULL;
  Points *ppre=NULL;

#if TEST
  FILE   *handle;
#endif

  double umin, umax,du,vmin,vmax,dv;
  double tab_u[UV_STEPS+1], tab_v[UV_STEPS+1], tab_lu[UV_STEPS+1], tab_lv[UV_STEPS+1];
  Points tab_p[UV_STEPS+1];
  double dtab_u[UV_STEPS+1], dtab_v[UV_STEPS+1], dtab_umean[UV_STEPS+1], dtab_vmean[UV_STEPS+1];

  int   min_nod=0, anz_n, *snods=NULL, *pnt_flag=NULL, *nod_mesh2d=NULL;
  double dr, min_dr=MAX_INTEGER;

  if(shape[surf[nr].sh].type!=4)
  {
    printf(" ERROR: tr3u requires a related nurbs. No nurbs found for surf:%s\n",surf[nr].name);  return(-1);
  }
  if(surf[nr].npgn<1) return(-1);

  nurbsnr=shape[surf[nr].sh].p[0];
  patch=surf[nr].patch;
  {
    if(printFlag) printf("NURBS:%s\n",nurbs[nurbsnr].name);

    /* create a table where the u,v values correspond to real length in the middle of the nurbs */
    /* the triangulation will take place by using scaled u,v coordinates to avoid very bad shaped elements */
    /* get the u and v range by looking into the knots */
    umin=nurbs[nurbsnr].uknt[0];
    umax=nurbs[nurbsnr].uknt[nurbs[nurbsnr].u_nknt-1];
    du=(umax-umin)/(UV_STEPS-1);
    vmin=nurbs[nurbsnr].vknt[0];
    vmax=nurbs[nurbsnr].vknt[nurbs[nurbsnr].v_nknt-1];
    dv=(vmax-vmin)/(UV_STEPS-1);
#if TEST
    printf("surf:%s nurbs:%s u:%lf-%lf v:%lf-%lf\n", surf[nr].name, nurbs[nurbsnr].name, umin,umax,vmin,vmax);
#endif
    for(i=0; i<UV_STEPS; i++)
    {
      tab_u[i]=dtab_u[i]=umin+du*i;
      tab_v[i]=dtab_v[i]=vmin+dv*i;
      dtab_umean[i]=(umax+umin)*.5;
      dtab_vmean[i]=(vmax+vmin)*.5;
    }

    tab_lu[0]=0.;
    evalNurbs( nurbs, nurbsnr, UV_STEPS, dtab_u, dtab_vmean, tab_p);
    for(i=1; i<UV_STEPS; i++)
    {
      p0[0]=tab_p[i-1].px;
      p0[1]=tab_p[i-1].py;
      p0[2]=tab_p[i-1].pz;
      p1[0]=tab_p[i].px;
      p1[1]=tab_p[i].py;
      p1[2]=tab_p[i].pz;
      v_result(p0,p1,p0p1);
      tab_lu[i]=tab_lu[i-1]+v_betrag(p0p1);
    }

    tab_lv[0]=0.;
    evalNurbs( nurbs, nurbsnr, UV_STEPS, dtab_umean, dtab_v, tab_p);
    for(i=1; i<UV_STEPS; i++)
    {
      p0[0]=tab_p[i-1].px;
      p0[1]=tab_p[i-1].py;
      p0[2]=tab_p[i-1].pz;
      p1[0]=tab_p[i].px;
      p1[1]=tab_p[i].py;
      p1[2]=tab_p[i].pz;
      v_result(p0,p1,p0p1);
      tab_lv[i]=tab_lv[i-1]+v_betrag(p0p1);
    }


    /* get the coords of the trimming loops (uv-values) */
    /* first determine the amount of points over all trimming loops for that patch (surface) */
    np=0; for(i=0; i<nurbs[nurbsnr].nc[patch]; i++) np+=nurbs[nurbsnr].np[patch][i];
    
    if( (pnt_u = (double *)malloc( (np+1)*sizeof(double) )) == NULL )
    { printf(" ERROR: malloc failure11 in repSurf(), nurbs:%s can not be shaped\n\n", nurbs[nurbsnr].name);
      return(-1); }
    if( (pnt_v = (double *)malloc( (np+1)*sizeof(double) )) == NULL )
    { printf(" ERROR: malloc failure11 in repSurf(), nurbs:%s can not be shaped\n\n", nurbs[nurbsnr].name);
      return(-1); }
    if( (npc = (int *)malloc( (nurbs[nurbsnr].nc[patch]+1)*sizeof(int) )) == NULL )
    { printf(" ERROR: malloc failure11 in repSurf(), nurbs:%s can not be shaped\n\n", nurbs[nurbsnr].name);
      return(-1); }

    np=k=0;
    for(i=0; i<nurbs[nurbsnr].nc[patch]; i++)
    {
      npc[k]=nurbs[nurbsnr].np[patch][i]-1;

      /* get the uv-values of the single curves which trim a surface */
      /* and scale them into real world length */
      n=p=0;
      for(j=0; j<npc[k]; j++)
      {
        pnt_u[np]=nurbs[nurbsnr].uv[patch][i][n++];
        pnt_v[np]=nurbs[nurbsnr].uv[patch][i][n++];

        pnt_u[np]=intpol( tab_u, tab_lu, UV_STEPS, pnt_u[np] );
        pnt_v[np]=intpol( tab_v, tab_lv, UV_STEPS, pnt_v[np] );

#if TEST
        printf("patch%d i%d j%d np%d uv:%lf %lf xyz:%lf %lf %lf\n",patch,i,j,np, pnt_u[j], pnt_v[j], nurbs[nurbsnr].xyz[patch][i][p]*scale->w+scale->x, nurbs[nurbsnr].xyz[patch][i][p+1]*scale->w+scale->y,  nurbs[nurbsnr].xyz[patch][i][p+2]*scale->w+scale->z); 
#endif
        p+=3;
        //if((np)&&(pnt_u[np]==pnt_u[np-1])&&(pnt_v[np]==pnt_v[np-1]));
        np++;
      }
      k++;
    }

    /* mesh the surface with triangles */
    e=mesh2d(&np, k, npc, &pnt_u, &pnt_v, &pnt_flag, &tri, meshp.alpha, meshp.beta, meshp.nadapt );
    if(e==0) goto no_elems;

    /* determine the coordinates of the triangles */
    /* first back into real u,v-coordinates */
    for(i=0; i<np; i++)
    {
      pnt_u[i]=intpol( tab_lu, tab_u, UV_STEPS, pnt_u[i] );
      if(pnt_u[i]>umax) pnt_u[i]=umax;
      else if(pnt_u[i]<umin) pnt_u[i]=umin;
      pnt_v[i]=intpol( tab_lv, tab_v, UV_STEPS, pnt_v[i] );
      if(pnt_v[i]>vmax) pnt_v[i]=vmax;
      else if(pnt_v[i]<vmin) pnt_v[i]=vmin;
    }
    if( (ppre = (Points *)malloc( (np+1)*sizeof(Points) )) == NULL )
    { printf(" ERROR: malloc failure11 in repSurf(), nurbs:%s can not be shaped\n\n", nurbs[nurbsnr].name);
      return(-1); }
    evalNurbs( nurbs, nurbsnr, np, pnt_u, pnt_v, ppre);

/* goto no_elems; */
    /* create nodes */
    /* store all node-numbers (not indexes) used by the surf in an array snods[j] */
    if( ( snods= (int *)malloc( (np*2)*sizeof(int) )) == NULL )
    { printf(" ERROR: malloc failure11 in repSurf(), nurbs:%s can not be shaped\n\n", nurbs[nurbsnr].name);
      return(-1); }
/* goto no_elems; geht noch  */
    anz_n=0;
    for (n=0; n<surf[nr].nl; n++)
    {
      k=surf[nr].l[n];
      if( surf[nr].typ[n]=='c' )
      {
        for( l=0; l<lcmb[k].nl; l++ )
        {
          m=lcmb[k].l[l];
          for(i=0; i<line[m].nn; i++) snods[anz_n++]=line[m].nod[i];
          snods[anz_n++]=point[line[m].p1].nod[0];
          snods[anz_n++]=point[line[m].p2].nod[0];
        }
      }
      else
      {
          for(i=0; i<line[k].nn; i++) snods[anz_n++]=line[k].nod[i];
          snods[anz_n++]=point[line[k].p1].nod[0];
          snods[anz_n++]=point[line[k].p2].nod[0];
      }
    }
/* goto no_elems; problem */

   /* check if the node exists already (line, point) or create a new one */
    if( ( nod_mesh2d= (int *)malloc( (np+1)*sizeof(int) )) == NULL )
    { printf(" ERROR: malloc failure11 in repSurf(), nurbs:%s can not be shaped\n\n", nurbs[nurbsnr].name);
      return(-1); }

    /* allocate memory for embeded nodes */
    if( ( surf[nr].nod=(int *)realloc( (int *)surf[nr].nod, (np+1)*sizeof(int)) )==NULL)
    { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[nr].name);
      return(-1); }
/* goto no_elems; */

    n=0; for(i=0; i<np; i++)
    {
      /* if its a boundary node (pnt_flag=1) search the original one */ 
      
      if(pnt_flag[i]!=0)
      {
        min_dr=MAX_INTEGER;
        for(j=0; j<anz_n; j++)
        {
          dr=(npre[snods[j]].nx-ppre[i].px)*(npre[snods[j]].nx-ppre[i].px)+(npre[snods[j]].ny-ppre[i].py)*(npre[snods[j]].ny-ppre[i].py)+(npre[snods[j]].nz-ppre[i].pz)*(npre[snods[j]].nz-ppre[i].pz);
          if(dr<min_dr){ min_dr=dr; min_nod=j; }
          /* printf("%f snods:%d min_dr:%lf min_nod:%d \n", dr,snods[min_nod],min_dr,min_nod ); */
        }
        nod_mesh2d[i+1]=snods[min_nod];
        /* printf("%d snods:%d min_dr:%lf min_nod:%d \n", i+1,snods[min_nod],min_dr,min_nod );
        printf("node:%d %f %f %f\n", snods[min_nod], npre[snods[min_nod]].nx, npre[snods[min_nod]].ny, npre[snods[min_nod]].nz );
        printf("node:%d %f %f %f\n", i, ppre[i].px, ppre[i].py, ppre[i].pz); */
      }
      else
      {
        nod( apre, &npre, 0, apre->nmax+1,ppre[i].px, ppre[i].py, ppre[i].pz, 0 );
        nod_mesh2d[i+1]=apre->nmax;
        surf[nr].nod[n++]=apre->nmax;
      }
    }
    surf[nr].nn=n;
/* goto no_elems; */

    /* create elements */
    /* allocate memory for embeded elements */
    if((surf[nr].elem=(int *)realloc((int *)surf[nr].elem, (e+1)*sizeof(int)) )==NULL)
    { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->elem\n\n", surf[nr].name); return(-1); }

    surf[nr].ne=0;
    j=0; for(i=0; i<e; i++)
    {
      /* do not create elements which have 2 unique nodes */
      if((nod_mesh2d[tri[j+1]]!=nod_mesh2d[tri[j]])&&(nod_mesh2d[tri[j+2]]!=nod_mesh2d[tri[j]])&&(nod_mesh2d[tri[j+2]]!=nod_mesh2d[tri[j+1]]))
      {
        /* the triangles must be inverted if the product of the nurbs-ori and surf-ori is '-' */
        if(surf[nr].sho=='-') oprod=-1; else oprod=1;
        if(surf[nr].ori=='-') oprod*=-1; else oprod*=1;
        if(oprod==1)
        {
	  ebuf[0]=nod_mesh2d[tri[j]];
	  ebuf[1]=nod_mesh2d[tri[j+1]];
	  ebuf[2]=nod_mesh2d[tri[j+2]];
          elem_define( anz->emax+1, 7, ebuf, 0, surf[nr].eattr );
        }
        else
        {
	  ebuf[0]=nod_mesh2d[tri[j+2]];
	  ebuf[1]=nod_mesh2d[tri[j+1]];
	  ebuf[2]=nod_mesh2d[tri[j]];
          elem_define( anz->emax+1, 7, ebuf, 0, surf[nr].eattr );
        }
        surf[nr].elem[surf[nr].ne]=anz->emax;
        surf[nr].ne++;
      }
      j+=3;
    }

/* goto no_elems; */
#if TEST
    handle = fopen ("all.msh", "w");
    fprintf(handle, "*NODE, NSET=Nall\n");
    for(j=0; j< np; j++)
      fprintf(handle, "%d, %lf, %lf, %lf\n", j+1, ppre[j].px, ppre[j].py, ppre[j].pz);
    fprintf(handle, "*ELEMENT, TYPE=S3R, ELSET=Eall\n");
    j=0;for(i=0; i<e; i++)
    {
    	fprintf(handle, "%d, %d, %d, %d\n", i+1, tri[j], tri[j+1], tri[j+2]); j+=3;
    }
    fprintf(handle, "*END STEP\n");
    fclose(handle);
#endif

    /* free the temporary space */
 no_elems:;
    free(npc); free(pnt_u); free(pnt_v); free(tri); free(ppre); free(snods); free(pnt_flag); free(nod_mesh2d);
    pnt_u=NULL; 
    pnt_v=NULL; 
    tri=NULL;
    ppre=NULL; 
    snods=NULL;
    pnt_flag=NULL;
    nod_mesh2d=NULL;
  }
  if(surf[nr].ne<=0) return(-1);
  else return(surf[nr].ne);
}



int merge_nlocal( Rsort *rsort, Nodes *nlocal, Elements *ctri3, int anz_e, int anz_n, int indx, int lock, double tol )
{
  int i, j, k,l;
  int  nod2, nod;
  int *nodes=NULL;

  nod=rsort[indx].i;

  nodes= findCloseEntity('n', nlocal, NULL, rsort, indx, anz_n, tol);

  for (i=1; i<=nodes[0]; i++)
  {
    if (rsort[nodes[i]].i > 0)
    {
      nod2=rsort[nodes[i]].i;
      /* check if nod exists in related elements */
      for (j=0; j<anz_e; j++)
      { 
        for (k=0; k<3; k++)
        {
          if( ctri3[j].nod[k] == nod2 )
          {
            if (lock) for (l=0; l<3; l++)
            {
              if( ctri3[j].nod[l] == nod )
              {
                printf(" node %d can not replace %d in element without collapsing it\n", nod, nod2 );
                goto nextNode;
              }
            }
            /* change node in elems if that node is used  */
            ctri3[j].nod[k] =nod;            
          }
        }
      }
      rsort[nodes[i]].i=0;
    }
    nextNode:;
  }
  free(nodes);
  return(1);
}



int deleteFreeEdges(Nodes *node, int *snods, int anz_n, Elements *elems, int anz_e )
{
  int i,j,k,n,m,nnr, anz_edg=0, nodstring[1000], cur_edg=0, flag[2];
  int *edges=NULL, *edgesb, edge_flag;
  static Elements *cbeam=NULL;
  double p0[3],p1[3], p0p1[3], p1p2[3], sp;

  /* determine free edges */
  do
  {
    anz_edg=findCTri3Edges(elems, anz_e, &edges);
    edgesb=edges;
    
    /* go over all edges and delete all flat elements */
    edge_flag=0;
    for(i=0; i<anz_edg; i++)
    {
      v_result(&node[*edges].nx,&node[*(edges+2)].nx,p0p1);
      v_result(&node[*(edges+2)].nx,&node[*(edges+1)].nx,p1p2);
      v_norm(p0p1,p0);
      v_norm(p1p2,p1);
      sp=v_sprod(p0,p1);

      if(sp>MIN_ANGLE_TRI3) 
      { 
        edge_flag=1;
        /* mark the element for removal */
        elems[*(edges+3)].group=0;
      }
      edges+=5;
    }
    free(edgesb);
  
    j=0;
    for(i=0; i<anz_e; i++)
    {
      if(elems[i].group)
      {
        elems[j].group=1;
        elems[j].mat=1;
        elems[j].nod[0]=elems[i].nod[0];
        elems[j].nod[1]=elems[i].nod[1];
        elems[j++].nod[2]=elems[i].nod[2];
      }
    }
    anz_e=j;
  }while(edge_flag);
  
    
  /* determine free edges after some bad elems are deleted */
  anz_edg=findCTri3Edges(elems, anz_e, &edges);
  
  if ((cbeam = (Elements *)realloc( (Elements *)cbeam, (anz_edg+1)*sizeof(Elements)) ) == NULL )
  { printf(" ERROR: realloc failure in deleteFreeEdges\n\n");
    return(-1); }
  
  edgesb=edges;
  for(i=0; i<anz_edg; i++)
  {
    cbeam[i].nod[0]=*(edges++);
    cbeam[i].nod[1]=*(edges);
    edges+=2;
    edges+=2;
    cbeam[i].group=1;      /* is initially 1 and will be 0 if it was used */
    cbeam[i].mat=1;
  }
  free(edgesb);

  /* go over all edges and search a string of edges until all edges are treatened */
  for(i=0; i<anz_edg; i++)
  {
    if(cbeam[i].group)
    {
      /* search an edge with a common node (snods) and a free node (which is to be erased) */
      flag[0]=flag[1]=1;
      for(n=0; n<2; n++)
      {
        for(j=0; j<anz_n; j++)
        {
          if(snods[j]==cbeam[i].nod[n]) flag[n]=0; 
        }
      }

      /* check if one node is also in snods and the other not */
      /* this is the start of a string */
      if((flag[0]+flag[1])==1)
      {
        /* store the nodepos which will survive (exists in snods) */
        if(flag[0]==0) n=0; else n=1;

        /* store the last edge of the string (cbeam_nr)*/
        cur_edg=i;

        /***  start of the string assembly ***/
        nnr=0;
        found_next:;
        nodstring[nnr++]=cbeam[cur_edg].nod[n];

        /* mark the edge as used */
        cbeam[cur_edg].group=0;

        /* search the connected edges (at least one must exist) */
        /* all nodes which are not in snods are redefined by elnod[0] */
        for(j=0; j<anz_edg; j++)
        {
          for(m=0; m<2; m++) if((cbeam[j].group)&&(cbeam[cur_edg].nod[!n]==cbeam[j].nod[m]))
          {
            cur_edg=j;
            n=m;

            /* check if this edge is also in snods */
            for(k=0; k<anz_n; k++)
            {
              if(snods[k]==cbeam[j].nod[m]) { nodstring[nnr]=cbeam[j].nod[m]; goto found_end; }
            }
            goto found_next;
          }
        }
        found_end:;

        /* replace nodstring[1..nnr] nodes by nodstring[0] in all affected elements */
        for(j=0; j<anz_e; j++)
        {
          for(m=1; m<nnr; m++)
          {
            /* calc the dist between the snods and nodsting */
            /* take the nearest */
            if( ((node[nodstring[m]].nx-node[nodstring[0]].nx)*
                   (node[nodstring[m]].nx-node[nodstring[0]].nx)+
                   (node[nodstring[m]].ny-node[nodstring[0]].ny)*
                   (node[nodstring[m]].ny-node[nodstring[0]].ny)+
                   (node[nodstring[m]].nz-node[nodstring[0]].nz)*
                   (node[nodstring[m]].nz-node[nodstring[0]].nz))
                 <((node[nodstring[m]].nx-node[nodstring[nnr]].nx)*
                   (node[nodstring[m]].nx-node[nodstring[nnr]].nx)+
                   (node[nodstring[m]].ny-node[nodstring[nnr]].ny)*
                   (node[nodstring[m]].ny-node[nodstring[nnr]].ny)+
                   (node[nodstring[m]].nz-node[nodstring[nnr]].nz)*
                   (node[nodstring[m]].nz-node[nodstring[nnr]].nz)) ) k=0; else k=nnr;
            
            for(n=0; n<3; n++)
            {
              if(elems[j].nod[n]==nodstring[m])
              {
                elems[j].nod[n]=nodstring[k];
              }
            }
          }
        } 
      }
    }
  }
  return(anz_e);
}



int mesh_tr3g( int s )
{
  int i,j,k=0,l,m,n,e,p, anz_n, anz_e;
  int min_nod=0;
  double min_dr, dr;
  Nodes *nlocal=NULL;
  Elements *elocal=NULL;
  Rsort *rsort=NULL;
  int *snods=NULL;

  if(shape[surf[s].sh].type!=4) return(-1);
  if(surf[s].npgn<1) return(-1);

  /* allocate memory for local nodes */
  if ( (nlocal = (Nodes *)malloc( (surf[s].npgn+1) * sizeof(Nodes))) == NULL )
  { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[s].name);
    return(-1); }

  /* allocate memory for embeded nodes */
  if( ( surf[s].nod=(int *)realloc( (int *)surf[s].nod, (3*surf[s].npgn+1)*sizeof(int)) )==NULL)
  { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[s].name);
    return(-1); }

  /* allocate memory for embeded elements */
  if((surf[s].elem=(int *)realloc((int *)surf[s].elem, (surf[s].npgn+1)*sizeof(int)) )==NULL)
  { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->elem\n\n", surf[s].name); return(-1); }

  /* write the interiour of the surface */
  n=e=0;
  p=0;
  while((surf[s].npgn-n))
  {
    /* create temporary nodes */
    n++;
    j=surf[s].pgn[n++];
    n+=3;
    for(k=0; k<j; k++)
    {
      p++;
      nlocal[p].nx=surf[s].pgn[n];
      nlocal[p].ny=surf[s].pgn[n+1];
      nlocal[p].nz=surf[s].pgn[n+2];
      n+=3;
    }

    /* create the element with local nodes */
    if (surf[s].ori=='+')
    {
      if(j==3)
      {
        if((elocal=(Elements *)realloc( (Elements *)elocal,(e+2)*sizeof(Elements)))==NULL)
        { printf("ERROR: realloc failure\n\n"); return(-1); }
        elocal[e].group=1;
        elocal[e].mat=1;
        elocal[e].nod[0]=p-2;
        elocal[e].nod[1]=p-1;
        elocal[e].nod[2]=p;
      }
      else printf("wrong number of nodes, number not supported\n");
    }
    else
    {
      if(j==3)
      {
        if((elocal=(Elements *)realloc( (Elements *)elocal,(e+2)*sizeof(Elements)))==NULL)
        { printf("ERROR: realloc failure\n\n"); return(-1); }
        elocal[e].group=1;
        elocal[e].mat=1;
        elocal[e].nod[0]=p;
        elocal[e].nod[1]=p-1;
        elocal[e].nod[2]=p-2;
      }
      else printf("wrong number of nodes, number not supported\n");
    }
    surf[s].elem[e++]=anz->emax;
  }
  surf[s].ne=e;
  anz_e=e;

  /* merge local nodes */
  /* calculate all absolute r of all nodes and sort the indexes according to r */ 
  if ( (rsort = (Rsort *)malloc( (p+1) * sizeof(Rsort))) == NULL )
    printf("ERROR: realloc failed: Rsort\n\n" ); 
  for( i=0; i<p; i++) 
  { 
    nlocal[i+1].nr=0;
    rsort[i].r=sqrt(nlocal[i+1].nx*nlocal[i+1].nx+nlocal[i+1].ny*nlocal[i+1].ny+nlocal[i+1].nz*nlocal[i+1].nz);
    rsort[i].i=i+1;
  }
  qsort( rsort, p, sizeof(Rsort), (void *)compareRsort );
#if TEST
  for (i=0; i<p; i++)
    printf("node:%d n:%d r:%lf\n", i, rsort[i].i, rsort[i].r); 
#endif

  k=p;
  for(i=0; i<k; i++)
  {
    j = merge_nlocal( rsort, nlocal, elocal, e, k, i, 0, gtol/scale->w );
  }
#if TEST
  for (i=0; i<p; i++)
    printf("rsort-indx:%d node:%d r:%lf\n", i, rsort[i].i, rsort[i].r); 
#endif

  /* create real nodes from the remaining ones */
  /* store all node-numbers (not indexes) used by the surf-lines in an array snods[j] */
  if( ( snods= (int *)malloc( (p+1)*sizeof(int) )) == NULL )
  { printf(" ERROR: malloc failure in meshGLU: surf %s\n\n", surf[s].name);
    return(-1); }
  anz_n=0;
  for (n=0; n<surf[s].nl; n++)
  {
    k=surf[s].l[n];
    if( surf[s].typ[n]=='c' )
    {
      for( l=0; l<lcmb[k].nl; l++ )
      {
        m=lcmb[k].l[l];
        if(surf[s].o[n]=='+')
        {
          if(lcmb[k].o[l]=='+')
          {
            snods[anz_n++]=point[line[m].p1].nod[0];
            for(i=0; i<line[m].nn; i++) snods[anz_n++]=line[m].nod[i];
          }
          else
          {
            snods[anz_n++]=point[line[m].p2].nod[0];
            for(i=line[m].nn-1; i>=0; i--) snods[anz_n++]=line[m].nod[i];
          }
	}
        else
        {
          if(lcmb[k].o[l]=='-')
          {
            snods[anz_n++]=point[line[m].p1].nod[0];
            for(i=0; i<line[m].nn; i++) snods[anz_n++]=line[m].nod[i];
          }
          else
          {
            snods[anz_n++]=point[line[m].p2].nod[0];
            for(i=line[m].nn-1; i>=0; i--) snods[anz_n++]=line[m].nod[i];
          }
	}
      }
    }
    else
    {
      if(surf[s].o[n]=='+')
      {
        snods[anz_n++]=point[line[k].p1].nod[0];
        for(i=0; i<line[k].nn; i++) snods[anz_n++]=line[k].nod[i];
      }
      else
      {
        snods[anz_n++]=point[line[k].p2].nod[0];
        for(i=line[k].nn-1; i>=0; i--) snods[anz_n++]=line[k].nod[i];
      }
    }
  }

  /* if a node matches a node in the trimming lines or points (snods) then change to this node */
  /* else create a new one */
  for(i=0; i<anz_n; i++)
  {
    min_dr=MAX_INTEGER;
    for(j=0; j<p; j++)
    {
      if(rsort[j].i!=0)
      {
        dr=(npre[snods[i]].nx-nlocal[rsort[j].i].nx)*(npre[snods[i]].nx-nlocal[rsort[j].i].nx)+(npre[snods[i]].ny-nlocal[rsort[j].i].ny)*(npre[snods[i]].ny-nlocal[rsort[j].i].ny)+(npre[snods[i]].nz-nlocal[rsort[j].i].nz)*(npre[snods[i]].nz-nlocal[rsort[j].i].nz);
        if(dr<min_dr){ min_dr=dr; min_nod=j; }
      }
    }
    /* printf("%d snods:%d min_dr:%lf min_nod:%d n:%d\n", i+1,snods[i],min_dr,min_nod, rsort[min_nod].i ); */
    nlocal[rsort[min_nod].i].nr=snods[i];
    rsort[min_nod].i=0;
  }
  n=0;
  for(i=0; i<p; i++)
  {
    if(rsort[i].i!=0)
    {
      nod( apre, &npre, 0, apre->nmax+1,nlocal[rsort[i].i].nx,nlocal[rsort[i].i].ny,nlocal[rsort[i].i].nz, 0 );
      nlocal[rsort[i].i].nr=apre->nmax;
      surf[s].nod[n++]=nlocal[rsort[i].i].nr;
    }
  }
  surf[s].nn=n;

  /* Delete additional nodes on the lines of the surf and replace affected elems */
  /* Only the original nodes on the lines are allowed. */
  /* Then all remaining elements between surfs are connected */

  /* update the node-numbers in the elements */
  for(i=0; i<anz_e; i++)
  {
    for(n=0; n<3; n++)
    {
      elocal[i].nod[n]=nlocal[elocal[i].nod[n]].nr;
    }
  }

  anz_e=deleteFreeEdges(npre, snods,anz_n,elocal,anz_e );

  /* delete collapsed elements */
  for(i=0; i<anz_e; i++)
  {
    if(elocal[i].nod[0]==elocal[i].nod[1]) elocal[i].group=0;
    else if(elocal[i].nod[0]==elocal[i].nod[2]) elocal[i].group=0;
    else if(elocal[i].nod[1]==elocal[i].nod[2]) elocal[i].group=0;
  }

  /* create final elements only for not collapsed ones */
  surf[s].ne=0;
  for(i=0; i<anz_e; i++)
  {
    if((elocal[i].nod[0]!=elocal[i].nod[1])&&(elocal[i].nod[0]!=elocal[i].nod[2])&&(elocal[i].nod[1]!=elocal[i].nod[2]))
    {
      if (surf[s].ori=='+')
      {
        ebuf[0]=elocal[i].nod[0];
        ebuf[1]=elocal[i].nod[1];
        ebuf[2]=elocal[i].nod[2];
        elem_define( anz->emax+1, 7, ebuf, 0, surf[s].eattr );
      }
      else
      {
        ebuf[0]=elocal[i].nod[2];
        ebuf[1]=elocal[i].nod[1];
        ebuf[2]=elocal[i].nod[0];
        elem_define( anz->emax+1, 7, ebuf, 0, surf[s].eattr );
      }
      surf[s].elem[surf[s].ne++]=anz->emax;
    }
  }

  free(snods);
  free(rsort);
  free(nlocal);
  free(elocal);

  /* reallocate memory for embeded nodes */
  if( ( surf[s].nod=(int *)realloc( (int *)surf[s].nod, (surf[s].nn+1)*sizeof(int)) )==NULL)
  { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[s].name);
    return(-1); }

  /* rellocate memory for embeded elements */
  if((surf[s].elem=(int *)realloc((int *)surf[s].elem, (surf[s].ne+1)*sizeof(int)) )==NULL)
  { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->elem2\n\n", surf[s].name); return(-1); }

  return(1);
}


int meshSurfs( int setNr )
{
  int noSurfMesh=0;
  int i,ii,j,jj,k,l,n,m, s=0, u,v, b=0,a, amax,bmax;
  static int *div_l=NULL;                 /* aufsummierte div der surf-edges */
  int sum_div, umax, vmax, imax, jmax;
  int   *n_uv=NULL, meshflag, mapflag;
  double *x=NULL, *y=NULL, *z=NULL;

  int   div_a,div_b, sa1,sa2,sb1,sb2;
  int   *n_ba=NULL, offs_sa1, offs_sa2;

  /* variables for the mesh-improver */
  static int   *n_indx=NULL, *n_ori=NULL, **e_nod=NULL, *n_type=NULL, **n_edge=NULL, **s_div=NULL;
  static double **n_coord=NULL; 

  int anz_s, lnew[2], ps, cl[2];
  char typnew[2];
  int edge[MAX_EDGES_PER_SURF];                 /* lines/lcmb for the meshable substitute surface with 4 edges */
  char  ctyp[MAX_EDGES_PER_SURF];      /*   type: l=line c=lcmb */
  char  cori[MAX_EDGES_PER_SURF];      /*   l-orient +- */
  char name[MAX_LINE_LENGTH];

  char surFlag;

  GLdouble *fptr;
  double v1[3], v2[3], vn[3];

  int transitionflag, ipuf=0, nori[2], eori[2], ebuf[26];

  Gsur surfbuf[1];

  int Stmp=-1, sh_buf=0;

  /* check if bodies based on this surf will be meshed */
  meshflag=0;
  for (i=0; i<set[setNr].anz_b; i++)
  {
    if(( body[set[setNr].body[i]].etyp==1)||( body[set[setNr].body[i]].etyp==4)) meshflag=1;
  }

  /* copy the amount of surfaces to be meshed, this is necessary because during mesh new surfs */
  /* could be created which should not be meshed (substitute surfs for 3- and 5-sided surfs)   */
  anz_s=set[setNr].anz_s;

  for (i=0; i<anz_s; i++)
  {
    j=set[setNr].surf[i];

    /* check if the lines for this surf are meshed */
    for(k=0; k<surf[j].nl; k++)
    {
      if(surf[j].typ[k]=='l') if(line[surf[j].l[k]].fail==1) goto noEtypDefined; 
    }

    /* if the interiour of the surf should be filled (no mesh), save the mesh */
    if (fillSurfFlag)
    {
      meshflag=1;

      if(surf[j].ne>0)
      {
        if((surfbuf[0].elem=(int *)malloc((surf[j].ne)*sizeof(int)) )==NULL)
        { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->elem (5)\n\n", surf[j].name); goto noMesh; }
        for(k=0; k<surf[j].ne; k++) surfbuf[0].elem[k]=surf[j].elem[k];
        surfbuf[0].ne=surf[j].ne;
      }
      else surfbuf[0].ne=0;
      if(surf[j].nn>0)
      {
        if((surfbuf[0].nod=(int *)malloc((surf[j].nn)*sizeof(int)) )==NULL)
        { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->nod (2)\n\n", surf[s].name); goto noMesh; }
        for(k=0; k<surf[j].nn; k++) surfbuf[0].nod[k]=surf[j].nod[k];
        surfbuf[0].nn=surf[j].nn;
      }
      else surfbuf[0].nn=0;
    }

    if((surf[j].etyp==7)&&(surf[j].sh<0)&&(surf[j].eattr==-1))
    {
      /* if shape=BLEND and the surf is plane, generate shape */
      if(surfToShape(j)>-1) 
      { if(printFlag) printf (" define shape for surf:%s\n", surf[j].name); }
    }

    if((surf[j].etyp==7)&&(surf[j].sh!=-1)&&(surf[j].eattr==-1))
    {
      if(printFlag) printf (" unstructured meshing of surf:%s\n", surf[j].name);

      /* if BLEND, generate shape */
      /* if shape, generate prelim nurbs */
      sh_buf=-1;
      if(shape[surf[j].sh].type==0)
      {
        /* generate a nurbs based on the shape */
        //Stmp= shapeToNurs(surf[j].sh);
        Stmp= surfToNurs(j);
    
        if(Stmp>-1)
        {
          pre_seta( "-Stmp", "S", nurbs[Stmp].name );
          completeSet( "-Stmp", "do" );
    
          sh_buf=surf[j].sh;
          surf[j].sh=shape_i( nurbs[Stmp].name, 4, Stmp, 0, 0);
          if(surf[j].pgn!=NULL)
          {
            free(surf[j].pgn); surf[j].pgn=NULL; surf[j].npgn=0;
          }
          if(printFlag) printf (" interior changed to Nurbs: %s\n", nurbs[Stmp].name );
          repSurf(j);
        }
      }
      ps=mesh_tr3u(j);

      /* restore the pointer to the shape */
      if(sh_buf>-1)
      {
        surf[j].sh=sh_buf;
        v=getSetNr("-Stmp");
        delNurs( set[v].anz_nurs, set[v].nurs ); 
        delPnt( set[v].anz_p, set[v].pnt ); 
        delSet("-Stmp");
      }

      if (ps==-1) goto noEtypDefined; 
      if ((nbuf = (int **)realloc((int **)nbuf, (apre->nmax+1)*sizeof(int *)) ) == NULL )
      { printf(" ERROR: realloc failure in meshSurf, nodes not installed\n\n"); return(-1); }
      for (v=sum_nbuf; v<=apre->nmax; v++)
      {
        if ((nbuf[v] = (int *)malloc( (2)*sizeof(int)) ) == NULL )
        { printf(" ERROR: realloc failure in meshSurf, nodes not installed\n\n"); return(-1); }
        nbuf[v][0]=0;
      }
      sum_nbuf=apre->nmax+1;
      goto checkSurf;
    }

    if((surf[j].etyp==7)&&(surf[j].sh>-1)&&(surf[j].eattr==-2))
    {
      if(printFlag) printf (" GLU based meshing of surf:%s\n", surf[j].name);
      ps=mesh_tr3g(j);
      if (ps==-1) goto noEtypDefined;
      if ((nbuf = (int **)realloc((int **)nbuf, (apre->nmax+1)*sizeof(int *)) ) == NULL )
      { printf(" ERROR: realloc failure in meshSurf, nodes not installed\n\n"); return(-1); }
      for (v=sum_nbuf; v<=apre->nmax; v++)
      {
        if ((nbuf[v] = (int *)malloc( (2)*sizeof(int)) ) == NULL )
        { printf(" ERROR: realloc failure in meshSurf, nodes not installed\n\n"); return(-1); }
        nbuf[v][0]=0;
      }
      sum_nbuf=apre->nmax+1;
      goto checkSurf;
    }
    else if (((surf[j].etyp==7)||
        (surf[j].etyp==8)||
        (surf[j].etyp==9)||
	 (surf[j].etyp==10)||(meshflag))&&(surf[j].nc==1))
    {
      surf[j].fail=1;
      if(printFlag) printf (" meshing surf:%s\n", surf[j].name);
    }
    else
      goto nextSurf;


    if (surf[j].nl!=MAX_EDGES_PER_SURF)
    {
      mapflag=1;
      if (surf[j].nl==2)
      {
        ps=splitLineAtDivratio( surf[j].l[0], surf[j].typ[0], 0.5, edge, ctyp);
        if (ps==-1) goto noEtypDefined;
        ps=splitLineAtDivratio( surf[j].l[1], surf[j].typ[1], 0.5, &edge[2], &ctyp[2]);
        if (ps==-1) goto noEtypDefined;

        /* create a 4 sided surf */
        for (jj=0; jj<4; jj+=2) { cori[jj]=cori[jj+1]=surf[j].o[jj]; }
#if TEST
        for (jj=0; jj<MAX_EDGES_PER_SURF; jj++)
        {
          if(ctyp[jj]=='l') printf("edge:%s cori:%c ctyp:%c\n", line[edge[jj]].name, cori[jj], ctyp[jj]);
          else if(ctyp[jj]=='c') printf("edge:%s cori:%c ctyp:%c\n", lcmb[edge[jj]].name, cori[jj], ctyp[jj]);
          else printf("error:%c\n",ctyp[jj]);
	}
#endif
        getNewName( name, "s" );
        s=surface_i( name, surf[j].ori, surf[j].sh, (int)4, cori, edge, ctyp );
        if( s<0)
        { printf("ERROR: surface could not be created\n"); goto noEtypDefined; }
        setr( 0, "s",s );        
        pre_seta(specialset->zap, "s", name);
        surf[s].etyp=surf[j].etyp;
        surf[s].eattr=surf[j].eattr;
      
        if(printFlag) printf("surf[%d]:%s is replaced by surf[%d]:%s\n",j, surf[j].name,s, surf[s].name);
        j=s;   
      }
      else if (surf[j].nl==3)
      {
        /* choose an edge with has no remainder for the equation division/4 in case elements have quadratic formulation */
        for (ii=0; ii<3; ii++)
        {
          if(( surf[j].typ[ii]=='l')&&(!(line[surf[j].l[ii]].div%4))) break;
          else if( line[surf[j].l[ii]].typ=='c')
          {
            sum_div=0;
            for(jj=0; jj<lcmb[surf[j].l[ii]].nl; jj++) sum_div+=line[lcmb[surf[j].l[ii]].l[jj]].div;
            if (!(sum_div%4)) break;
	  }
        }
        /* no mesh if no suitable edge exists */
        if(ii==3) ii=2;
        ps=splitLineAtDivratio( surf[j].l[ii], surf[j].typ[ii], 0.5, lnew, typnew);
        if (ps==-1) goto noEtypDefined;
        /* create a 4 sided surf */
        for (jj=0; jj<ii; jj++)
        {
          edge[jj]=surf[j].l[jj];
          cori[jj]=surf[j].o[jj];
          ctyp[jj]=surf[j].typ[jj];
        }
        edge[jj]=lnew[0];
        cori[jj]=surf[j].o[ii];
        ctyp[jj]=typnew[0];
        jj++;
        edge[jj]=lnew[1];
        cori[jj]=surf[j].o[ii];
        ctyp[jj]=typnew[1];
        for (; jj<3; jj++) /* thats ok so */
        {
          edge[jj+1]=surf[j].l[jj];
          cori[jj+1]=surf[j].o[jj];
          ctyp[jj+1]=surf[j].typ[jj];
        }

#if TEST
        for (jj=0; jj<MAX_EDGES_PER_SURF; jj++)
        {
          if(ctyp[jj]=='l') printf("edge:%s cori:%c ctyp:%c\n", line[edge[jj]].name, cori[jj], ctyp[jj]);
          else if(ctyp[jj]=='c') printf("edge:%s cori:%c ctyp:%c\n", lcmb[edge[jj]].name, cori[jj], ctyp[jj]);
          else printf("error:%c\n",ctyp[jj]);
	}
#endif
        getNewName( name, "s" );
        s=surface_i( name, surf[j].ori, surf[j].sh, (int)4, cori, edge, ctyp );
        if( s<0)
        { printf("ERROR: surface could not be created\n"); goto noEtypDefined; }
        pre_seta(specialset->zap, "s", name);
        setr( 0, "s",s );        
        surf[s].etyp=surf[j].etyp;
        surf[s].eattr=surf[j].eattr;

        /* allocate memory for the reference to the substitute-surface (s)  */
        if((surf[j].l=(int *)realloc((int *)surf[j].l, (surf[j].nl+7)*sizeof(int)) )==NULL)
        { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->l\n\n", surf[j].name);
          return(-1); }
        surf[j].l[surf[j].nl]=ii;
        surf[j].l[surf[j].nl+1]=s;
        surf[j].l[surf[j].nl+2]=ps;
        surf[j].l[surf[j].nl+3]=lnew[0];
        surf[j].l[surf[j].nl+4]=typnew[0];
        surf[j].l[surf[j].nl+5]=lnew[1];
        surf[j].l[surf[j].nl+6]=typnew[1];
      
        if(printFlag) printf("surf[%d]:%s is replaced by surf[%d]:%s\n",j, surf[j].name,s, surf[s].name);
        j=s;   
      }

      /* to mesh a n-sided surf two corners will be combined to one lcmb until only 4 edges remain */
      else if (surf[j].nl>4)
      {
        /* create a temporary surface for the meshing */
        getNewName( name, "s" );
        s=surface_i( name, surf[j].ori, surf[j].sh, surf[j].c[0], surf[j].o, surf[j].l, surf[j].typ );
        if( s<0)
        { printf("ERROR: surface could not be created\n"); goto noEtypDefined; }
        pre_seta(specialset->zap, "s", name);
        setr( 0, "s",s );        
        surf[s].etyp=surf[j].etyp;
        surf[s].eattr=surf[j].eattr;

        for (ii=0; ii<surf[j].nl-MAX_EDGES_PER_SURF; ii++)
        {
          /* determine the best suited corner for the lcmb */
          determineBestCorners( s,cl);
#if TEST
	  printf("edges:%d\n", surf[s].nl);
          for (jj=0; jj<2; jj++)
	  {          
	    if(surf[s].typ[cl[jj]]=='l') printf("add:%d %s %c\n",cl[jj], line[surf[s].l[cl[jj]]].name, surf[s].o[cl[jj]]);
            else  printf("add:%d %s %c\n",cl[jj], lcmb[surf[s].l[cl[jj]]].name, surf[s].o[cl[jj]]);
	  } 
#endif
          /* create a lcmb out of 2 edges */
          lnew[0]=addTwoLines( surf[s].l[cl[0]], surf[s].o[cl[0]], surf[s].typ[cl[0]], surf[s].l[cl[1]] ,surf[s].o[cl[1]], surf[s].typ[cl[1]] );
          if( lnew[0]==-1) goto noEtypDefined;

          /* replace the concatonated lines of the surface */
          if (cl[0]==surf[s].nl-1)
          {
            surf[s].l[0]=lnew[0];
            surf[s].o[0]='+';
            surf[s].typ[0]='c';
            surf[s].nl--;
          }
          else
          {
            surf[s].l[cl[0]]=lnew[0];
            surf[s].o[cl[0]]='+';
            surf[s].typ[cl[0]]='c';
            for (jj=cl[0]+1; jj<surf[s].nl-1; jj++)
            {
              surf[s].l[jj]=surf[s].l[jj+1];
              surf[s].o[jj]=surf[s].o[jj+1];
              surf[s].typ[jj]=surf[s].typ[jj+1];
            }
            surf[s].nl--;
          }
	}
        if(printFlag) printf("surf[%d]:%s is replaced by surf[%d]:%s\n",j, surf[j].name,s, surf[s].name);
#if TEST
        for (jj=0; jj<MAX_EDGES_PER_SURF; jj++)
        {
          if(surf[s].typ[jj]=='l') printf("edge:%s cori:%c ctyp:%c\n", line[surf[s].l[jj]].name, surf[s].o[jj], surf[s].typ[jj]);
          else if(surf[s].typ[jj]=='c') printf("edge:%s cori:%c ctyp:%c\n", lcmb[surf[s].l[jj]].name, surf[s].o[jj], surf[s].typ[jj]);
          else printf("error:%c\n",surf[s].typ[jj]);
	}
#endif
        if (surf[j].nl==5)
        {
          /* allocate memory for the reference to the substitute-surface (s)  */
          if((surf[j].l=(int *)realloc((int *)surf[j].l, (surf[j].nl+4)*sizeof(int)) )==NULL)
          { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->l (2)\n\n", surf[j].name);
            return(-1); }
          surf[j].l[surf[j].nl]=cl[0];    /* place of the line in the surf */
          surf[j].l[surf[j].nl+1]=cl[1];  /* place of the line in the surf */
          surf[j].l[surf[j].nl+2]=s;  /* slave surf */
          surf[j].l[surf[j].nl+3]=lnew[0]; /* slave lcmb */
	}
        j=s;   
      }
      else
      {
        printf(" ERROR: in meshSurfs Surf:%s has no valid number of edges (has:%d)\n", surf[j].name, surf[j].nl );
        goto noEtypDefined;
      }
    }
    else  mapflag=0;
       


    /* determine the amount of nodes in the surf without the nodes of the borderlines and points */
    /* determine the div of each edge */
    /* and check if all lines of that surf are meshed (canceled:not nessesary up to now! )*/
    if( (div_l=(int *)realloc((int *)div_l, (surf[j].nl+1)*sizeof(int) ) )==NULL) 
    { printf(" ERROR: realloc failure in meshSurfs()\n"); return(-1); }

    for (n=0; n<surf[j].nl; n++)
    {
      k=surf[j].l[n];
      div_l[n]=0;
      if( surf[j].typ[n]=='c' )
      {
        for( l=0; l<lcmb[k].nl; l++ )
        {
          m=lcmb[k].l[l];
          div_l[n]+=line[m].div;
        }
      }
      else
        div_l[n]=line[k].div;
    }

    /* look if the divisions are suited for the elemtype  */
    /*  sum_div%2=0 for linear elements */
    sum_div=0;
    if ((surf[j].etyp==7)||(surf[j].etyp==9))
    {
      for (n=0; n<4; n++) sum_div+=div_l[n];
      if((sum_div&(int)1))
      {
        printf (" WARNING: bad divisions in surf:%s\n", surf[set[setNr].surf[i]].name);
        printf ("  sum_div:%d sum_div&(int)1:%d \n", sum_div, (sum_div&(int)1));
        goto noEtypDefined;
      } 
    }
    /* check each div%2=0 for quadratic elems, and check if the sum_div%4=0 */
    if ((surf[j].etyp==8)||(surf[j].etyp==10))
    { 
      for (n=0; n<4; n++)
      {
        if((div_l[n]&(int)1))
        {
          printf (" WARNING: bad divisions in surf:%s\n", surf[set[setNr].surf[i]].name);
          goto noEtypDefined;
        } 
        sum_div+=div_l[n];
      } 
      if( ((sum_div&(int)1))||((sum_div&(int)2)) )
      {
        printf (" WARNING: bad divisions in surf:%s\n", surf[set[setNr].surf[i]].name);
        goto noEtypDefined;
      } 
    }

    /* check if the surf has balanced edges */
    if ((div_l[0]!=div_l[2])||(div_l[1]!=div_l[3]))
    {
      transitionflag=1;

      /* surf has unbalanced edges, check if it is meshable */
      /* and calculate the nessesary divisions of the surf    */

      n=newDivisions( div_l, &div_a, &div_b, &sa1, &sa2, &sb1, &sb2 );
      if (n==-1)
      {
        printf("ERROR: surf:%s has bad divisions\n\n",surf[j].name);
        goto noEtypDefined;
      }

      /* Fuelle ein xyz-feld mit den koordinaten einer Flaeche mit ungleichen divisions. */
      /* Die xyz-Koordinaten gelten fuer ein Feld im uv-Raum bei dem die u-achse mit der*/
      /* surf[j].l[0] zusammenfaellt und die v-achse mit der surf[j].l[3].              */
      /* ausserdem werden die     knotenNr im feld n_ba und n_uv abgelegt               */

      if( (n_uv=(int *)malloc( (div_a+1)*(div_b+1)*sizeof(int) ) )==NULL)
      { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[j].name);
        return(-1); }
      if( (n_ba=(int *)malloc( (div_a+1)*(div_b+1)*sizeof(int) ) )==NULL)
      { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[j].name);
        return(-1); }
      if( (x=(double *)malloc( (div_a+1)*(div_b+1)*sizeof(double) ) )==NULL)
      { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[j].name);
        return(-1); }
      if( (y=(double *)malloc( (div_a+1)*(div_b+1)*sizeof(double) ) )==NULL)
      { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[j].name);
        return(-1); }
      if( (z=(double *)malloc( (div_a+1)*(div_b+1)*sizeof(double) ) )==NULL)
      { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[j].name);
        return(-1); }

      /* allocate memory for embeded nodes */
      if((surf[j].nod=(int *)realloc((int *)surf[j].nod, ((div_a)*(div_b))*sizeof(int)) )==NULL)
      { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->nod\n\n", surf[j].name);
        return(-1); }

      if( fillSurf2( j, div_l, div_a, div_b, sa1, sa2, sb1, sb2
      , n_uv, &umax, &vmax, n_ba, &amax, &bmax, &offs_sa1, &offs_sa2, x,y,z)<0)
        goto noEtypDefined;

      surf[j].fail=0;


      /* START of section mesh-improver, allocate memory for the mesh-improver */
      if ((surf[j].etyp==9)||(surf[j].etyp==10))
      {
        if((n_indx=(int *)realloc((int *)n_indx, (apre->nmax+1)*sizeof(int)) )==NULL)
        { printf(" ERROR: n_indx realloc failure in meshSurfs surf:%s can not be meshed\n\n"
          , surf[j].name); return(-1); }
        if((n_ori=(int *)realloc((int *)n_ori, ((div_a+1)*(div_b+1))*sizeof(int)) )==NULL)
        { printf(" ERROR: n_ori realloc failure in meshSurfs surf:%s can not be meshed\n\n"
          , surf[j].name); return(-1); }
        if((e_nod=(int **)realloc((int **)e_nod, ((div_a+1)*(div_b+1))*sizeof(int *)) )==NULL)
        { printf(" ERROR: e_nod realloc failure in meshSurfs surf:%s can not be meshed\n\n"
          , surf[j].name); return(-1); }
        for (ii=0; ii<((div_a+1)*(div_b+1)); ii++)
          if((e_nod[ii]=(int *)malloc( (int)20*sizeof(int)) )==NULL)
          { printf(" ERROR: e_nod[%d] malloc failure in meshSurfs surf:%s can not be meshed\n\n"
           , ii, surf[j].name); return(-1); }
        if((n_edge=(int **)realloc((int **)n_edge, (int)MAX_SURFS_PER_BODY*sizeof(int *)) )==NULL)
        { printf(" ERROR: n_edge realloc failure in meshSurfs surf:%s can not be meshed\n\n"
          , surf[j].name); return(-1); }
        for (ii=0; ii<MAX_SURFS_PER_BODY; ii++)
          if((n_edge[ii]=(int *)malloc( (int)((div_a+1)*(div_b+1))*sizeof(int)) )==NULL)
          { printf(" ERROR: n_edge[%d] malloc failure in meshSurfs surf:%s can not be meshed\n\n"
            , ii, surf[j].name); return(-1); }
        if((s_div=(int **)realloc((int **)s_div, (int)MAX_SURFS_PER_BODY*sizeof(int *)) )==NULL)
        { printf(" ERROR: s_div realloc failure in meshSurfs surf:%s can not be meshed\n\n"
          , surf[j].name); return(-1); }
        for (ii=0; ii<MAX_SURFS_PER_BODY; ii++)
          if((s_div[ii]=(int *)malloc( (int)MAX_EDGES_PER_SURF*sizeof(int)) )==NULL)
          { printf(" ERROR: s_div[%d] malloc failure in meshSurfs surf:%s can not be meshed\n\n"
           , ii, surf[j].name); return(-1); }
        if((n_type=(int *)realloc((int *)n_type, ((div_a+1)*(div_b+1)+1)*sizeof(int)) )==NULL)
        { printf(" ERROR: n_type realloc failure in meshSurfs surf:%s can not be meshed\n\n"
          , surf[j].name); return(-1); }
        if((n_coord=(double **)realloc((double **)n_coord, (int)((div_a+1)*(div_b+1))*sizeof(double *)) )==NULL)
        { printf(" ERROR: n_coord realloc failure in meshSurfs surf:%s can not be meshed\n\n"
          , surf[j].name); return(-1); }
        for (ii=0; ii<((div_a+1)*(div_b+1)); ii++)
          if((n_coord[ii]=(double *)malloc( (int)3*sizeof(double)) )==NULL)
          { printf(" ERROR: n_coord[%d] malloc failure in meshSurfs surf:%s can not be meshed\n\n"
            , ii, surf[j].name); return(-1); }
  
        /* ini data for mesh-improver */
        jj=0;
        for(ii=0; ii<=apre->nmax; ii++) n_indx[ii]=-1;      /* initialized as unused */
        for(ii=0; ii<((div_a+1)*(div_b+1)); ii++) n_type[ii]=1; /* initialized as surface nodes */ 
      }
      /* INTERRUPT of section mesh-improver */

      /* erzeugen der elemente  */
      k=0;

      /* allocate memory for final-node-buffer nbuf and final nodes */
      if ((nbuf = (int **)realloc((int **)nbuf, (apre->nmax+1)*sizeof(int *)) ) == NULL )
      { printf(" ERROR: realloc failure in meshSurf, nodes not installed\n\n"); return(-1); }
      for (v=sum_nbuf; v<=apre->nmax; v++)
      {
        if ((nbuf[v] = (int *)malloc( (2)*sizeof(int)) ) == NULL )
        { printf(" ERROR: realloc failure in meshSurf, nodes not installed\n\n"); return(-1); }
        nbuf[v][0]=0;
      }
      sum_nbuf=apre->nmax+1;
          
      if (fillSurfFlag)
      {
        /* if we have a substitute surf, fill the original one */
        if(mapflag) s=j; j=set[setNr].surf[i];
        surf[j].npgn=0;
        if( (surf[j].pgn=fptr= (GLdouble *)realloc( (GLdouble *)surf[j].pgn, (div_a*div_b*28)*sizeof(GLdouble) )) == NULL )
        { printf(" ERROR1: realloc failure in meshSurf()\n\n"); return(-1); }
        for (b=0; b<div_b  ; b++)
        {
          for (a=0; a<div_a; a++)
          {
            if ((a>=div_l[sa1])&&(b<offs_sa1)) goto nofill;
            if ((a>=div_l[sa2])&&(b>=div_b-offs_sa2)) goto nofill;
            if (surf[j].ori=='+')
            {
              *fptr=GL_POLYGON_TOKEN; fptr++;
              *fptr=3; fptr++;
              v_result(&npre[n_ba[(b  )*amax + a    ]].nx,&npre[n_ba[(b+1)*amax + a    ]].nx, v1); 
              v_result(&npre[n_ba[(b  )*amax + a    ]].nx,&npre[n_ba[(b  )*amax + (a+1)]].nx, v2);
              v_prod(v1,v2,vn);
              v_norm(vn,fptr); fptr+=3;
              *fptr=npre[n_ba[(b  )*amax + a    ]].nx; fptr++;
              *fptr=npre[n_ba[(b  )*amax + a    ]].ny; fptr++;
              *fptr=npre[n_ba[(b  )*amax + a    ]].nz; fptr++;

              *fptr=npre[n_ba[(b+1)*amax + a    ]].nx; fptr++;
              *fptr=npre[n_ba[(b+1)*amax + a    ]].ny; fptr++;
              *fptr=npre[n_ba[(b+1)*amax + a    ]].nz; fptr++;

              *fptr=npre[n_ba[(b  )*amax + (a+1)]].nx; fptr++;
              *fptr=npre[n_ba[(b  )*amax + (a+1)]].ny; fptr++;
              *fptr=npre[n_ba[(b  )*amax + (a+1)]].nz; fptr++;


              *fptr=GL_POLYGON_TOKEN; fptr++;
              *fptr=3; fptr++;
              v_result(&npre[n_ba[(b  )*amax + (a+1)]].nx,&npre[n_ba[(b+1)*amax + a    ]].nx, v1); 
              v_result(&npre[n_ba[(b  )*amax + (a+1)]].nx,&npre[n_ba[(b+1)*amax + (a+1)]].nx, v2);
              v_prod(v1,v2,vn);
              v_norm(vn,fptr); fptr+=3;
              *fptr=npre[n_ba[(b  )*amax + (a+1)]].nx; fptr++;
              *fptr=npre[n_ba[(b  )*amax + (a+1)]].ny; fptr++;
              *fptr=npre[n_ba[(b  )*amax + (a+1)]].nz; fptr++;
			 
              *fptr=npre[n_ba[(b+1)*amax + a    ]].nx; fptr++;
              *fptr=npre[n_ba[(b+1)*amax + a    ]].ny; fptr++;
              *fptr=npre[n_ba[(b+1)*amax + a    ]].nz; fptr++;
			 
              *fptr=npre[n_ba[(b+1)*amax + (a+1)]].nx; fptr++;
              *fptr=npre[n_ba[(b+1)*amax + (a+1)]].ny; fptr++;
              *fptr=npre[n_ba[(b+1)*amax + (a+1)]].nz; fptr++;
              surf[j].npgn+=28;
            }
            else
            {
              *fptr=GL_POLYGON_TOKEN; fptr++;
              *fptr=3; fptr++;
              v_result(&npre[n_ba[(b  )*amax + a    ]].nx,&npre[n_ba[(b  )*amax + (a+1)]].nx, v1); 
              v_result(&npre[n_ba[(b  )*amax + a    ]].nx,&npre[n_ba[(b+1)*amax + a    ]].nx, v2);
              v_prod(v1,v2,vn);
              v_norm(vn,fptr); fptr+=3;
              *fptr=npre[n_ba[(b  )*amax + a    ]].nx; fptr++;
              *fptr=npre[n_ba[(b  )*amax + a    ]].ny; fptr++;
              *fptr=npre[n_ba[(b  )*amax + a    ]].nz; fptr++;
			                         
              *fptr=npre[n_ba[(b  )*amax + (a+1)]].nx; fptr++;
              *fptr=npre[n_ba[(b  )*amax + (a+1)]].ny; fptr++;
              *fptr=npre[n_ba[(b  )*amax + (a+1)]].nz; fptr++;
			                         
              *fptr=npre[n_ba[(b+1)*amax + a    ]].nx; fptr++;
              *fptr=npre[n_ba[(b+1)*amax + a    ]].ny; fptr++;
              *fptr=npre[n_ba[(b+1)*amax + a    ]].nz; fptr++;

              *fptr=GL_POLYGON_TOKEN; fptr++;
              *fptr=3; fptr++;
              v_result(&npre[n_ba[(b  )*amax + (a+1)]].nx,&npre[n_ba[(b+1)*amax + (a+1)]].nx, v1); 
              v_result(&npre[n_ba[(b  )*amax + (a+1)]].nx,&npre[n_ba[(b+1)*amax + a    ]].nx, v2);
              v_prod(v1,v2,vn);
              v_norm(vn,fptr); fptr+=3;
              *fptr=npre[n_ba[(b  )*amax + (a+1)]].nx; fptr++;
              *fptr=npre[n_ba[(b  )*amax + (a+1)]].ny; fptr++;
              *fptr=npre[n_ba[(b  )*amax + (a+1)]].nz; fptr++;
			                         
              *fptr=npre[n_ba[(b+1)*amax + (a+1)]].nx; fptr++;
              *fptr=npre[n_ba[(b+1)*amax + (a+1)]].ny; fptr++;
              *fptr=npre[n_ba[(b+1)*amax + (a+1)]].nz; fptr++;
			                         
              *fptr=npre[n_ba[(b+1)*amax + a    ]].nx; fptr++;
              *fptr=npre[n_ba[(b+1)*amax + a    ]].ny; fptr++;
              *fptr=npre[n_ba[(b+1)*amax + a    ]].nz; fptr++;
              surf[j].npgn+=28;
            }
            nofill:;
          }
        }
        if(mapflag) j=s; /* change back to substitute surf */
      }
      else if (surf[j].etyp==7)
      {
        /* allocate memory for embeded elements */
        if((surf[j].elem=(int *)realloc((int *)surf[j].elem, (div_a*div_b*2)*sizeof(int)) )==NULL)
        { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->elem\n\n", surf[j].name);
          return(-1); }
        for (b=0; b<div_b  ; b++)
        {
          for (a=0; a<div_a; a++)
          {
            if ((a>=div_l[sa1])&&(b<offs_sa1)) goto noelem7;
            if ((a>=div_l[sa2])&&(b>=div_b-offs_sa2)) goto noelem7;
            if (surf[j].ori=='+')
            {
              ebuf[0]=n_ba[(b  )*amax + a    ];
              ebuf[1]=n_ba[(b+1)*amax + a    ];
              ebuf[2]=n_ba[(b  )*amax + (a+1)];
              elem_define( anz->emax+1, 7, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
              k++;
              ebuf[0]=n_ba[(b  )*amax + (a+1)];
              ebuf[1]=n_ba[(b+1)*amax + a    ];
              ebuf[2]=n_ba[(b+1)*amax + (a+1)];
              elem_define( anz->emax+1, 7, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
              k++;
            }
            else
            {
              ebuf[0]=n_ba[(b  )*amax + a    ];
              ebuf[1]=n_ba[(b  )*amax + (a+1)];
              ebuf[2]=n_ba[(b+1)*amax + a    ];
              elem_define( anz->emax+1, 7, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
              k++;
              ebuf[0]=n_ba[(b  )*amax + (a+1)];
              ebuf[1]=n_ba[(b+1)*amax + (a+1)];
              ebuf[2]=n_ba[(b+1)*amax + a    ];
              elem_define( anz->emax+1, 7, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
              k++;
            }
            noelem7:;
          }
        }
      }
      else if (surf[j].etyp==8)
      {
        /* allocate memory for embeded elements */
        if((surf[j].elem=(int *)realloc((int *)surf[j].elem, (div_a*div_b/2)*sizeof(int)) )==NULL)
        { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->elem (2)\n\n", surf[j].name);
          return(-1); }
        for (b=0; b<div_b-1; b+=2)
        {
          for (a=0; a<div_a-1; a+=2)
          {
            if ((a>=div_l[sa1])&&(b<offs_sa1)) goto noelem8;
            if ((a>=div_l[sa2])&&(b>=div_b-offs_sa2)) goto noelem8;
            if (surf[j].ori=='+')
            {
              ebuf[0]=n_ba[(b  )*amax + a    ];
              ebuf[1]=n_ba[(b+2)*amax + a    ];
              ebuf[2]=n_ba[(b  )*amax + (a+2)];
              ebuf[3]=n_ba[(b+1)*amax + (a  )];
              ebuf[4]=n_ba[(b+1)*amax + (a+1)];
              ebuf[5]=n_ba[(b  )*amax + (a+1)];
              elem_define( anz->emax+1, 8, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
              k++;
              ebuf[0]=n_ba[(b  )*amax + (a+2)];
              ebuf[1]=n_ba[(b+2)*amax + a    ];
              ebuf[2]=n_ba[(b+2)*amax + (a+2)];
              ebuf[3]=n_ba[(b+1)*amax + (a+1)];
              ebuf[4]=n_ba[(b+2)*amax + (a+1)];
              ebuf[5]=n_ba[(b+1)*amax + (a+2)];
              elem_define( anz->emax+1, 8, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
              k++;
            }
            else
            {
              ebuf[0]=n_ba[(b  )*amax + a    ];
              ebuf[1]=n_ba[(b  )*amax + (a+2)];
              ebuf[2]=n_ba[(b+2)*amax + a    ];
              ebuf[3]=n_ba[(b  )*amax + (a+1)];
              ebuf[4]=n_ba[(b+1)*amax + (a+1)];
              ebuf[5]=n_ba[(b+1)*amax + (a  )];
              elem_define( anz->emax+1, 8, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
              k++;
              ebuf[0]=n_ba[(b  )*amax + (a+2)];
              ebuf[1]=n_ba[(b+2)*amax + (a+2)];
              ebuf[2]=n_ba[(b+2)*amax + a    ];
              ebuf[3]=n_ba[(b+1)*amax + (a+2)];
              ebuf[4]=n_ba[(b+2)*amax + (a+1)];
              ebuf[5]=n_ba[(b+1)*amax + (a+1)];
              elem_define( anz->emax+1, 8, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
              k++;
            }
            noelem8:;
          }
        }
      }
      else if (surf[j].etyp==9)
      {
        /* allocate memory for embeded elements */
        if((surf[j].elem=(int *)realloc((int *)surf[j].elem, (div_a*div_b)*sizeof(int)) )==NULL)
        { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->elem (3)\n\n", surf[j].name);
          return(-1); }
        for (b=0; b<div_b  ; b++)
        {
          for (a=0; a<div_a; a++)
          {
            if ((a>=div_l[sa1])&&(b<offs_sa1)) goto noelem9;
            if ((a>=div_l[sa2])&&(b>=div_b-offs_sa2)) goto noelem9;
            if (surf[j].ori=='+')
            {
              ebuf[0]=n_ba[(b  )*amax + a    ];
              ebuf[1]=n_ba[(b+1)*amax + a    ];
              ebuf[2]=n_ba[(b+1)*amax + (a+1)];
              ebuf[3]=n_ba[(b  )*amax + (a+1)];
              elem_define( anz->emax+1, 9 , ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
            }
            else
            {
              ebuf[0]=n_ba[(b  )*amax + a    ];
              ebuf[1]=n_ba[(b  )*amax + (a+1)];
              ebuf[2]=n_ba[(b+1)*amax + (a+1)];
              ebuf[3]=n_ba[(b+1)*amax + a    ];
              elem_define( anz->emax+1, 9, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
            }

              /* RESTART of section mesh-improver, describe variables for the mesh-improver */
              for (ii=0; ii<4; ii++)
              {
                if( n_indx[e_enqire[anz->emax].nod[ii]]==-1) /* node not stored yet */
                {
                  n_ori[jj]=e_enqire[anz->emax].nod[ii];
                  n_coord[jj][0]=npre[n_ori[jj]].nx;
                  n_coord[jj][1]=npre[n_ori[jj]].ny;
                  n_coord[jj][2]=npre[n_ori[jj]].nz;
                  jj++;
                  n_indx[e_enqire[anz->emax].nod[ii]]=jj; /* first number is "1" to match the needs of the improver */
                }
#if TEST
		printf("en[i]:%d n_indx[en[i]]:%d k:%d i:%d\n", e_enqire[anz->emax].nod[ii],n_indx[e_enqire[anz->emax].nod[ii]],k,ii);
#endif
                e_nod[k][ii]=n_indx[e_enqire[anz->emax].nod[ii]];
              }
              /* INTERRUPT of section mesh-improver */

            k++;
            noelem9:;
          }
        }
      }      
      else if (surf[j].etyp==10)
      {
        /* allocate memory for embeded elements */
        if((surf[j].elem=(int *)realloc((int *)surf[j].elem, (div_a*div_b/4)*sizeof(int)) )==NULL)
        { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->elem (4)\n\n", surf[j].name);
          return(-1); }
        for (b=0; b<div_b  ; b+=2)
        {
          for (a=0; a<div_a; a+=2)
          {
            if ((a>=div_l[sa1])&&(b<offs_sa1)) goto noelem10;
            if ((a>=div_l[sa2])&&(b>=div_b-offs_sa2)) goto noelem10;
            if (surf[j].ori=='+')
            {
              ebuf[0]=n_ba[(b  )*amax + a    ];
              ebuf[4]=n_ba[(b+1)*amax + a    ];
              ebuf[1]=n_ba[(b+2)*amax + a    ];
              ebuf[5]=n_ba[(b+2)*amax + (a+1)];
              ebuf[2]=n_ba[(b+2)*amax + (a+2)];
              ebuf[6]=n_ba[(b+1)*amax + (a+2)];
              ebuf[3]=n_ba[(b)*amax   + (a+2)];
              ebuf[7]=n_ba[(b  )*amax + (a+1)];
              elem_define( anz->emax+1, 10, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
            }
            else
            {
              ebuf[0]=n_ba[(b  )*amax + a    ];
              ebuf[4]=n_ba[(b  )*amax + (a+1)];
              ebuf[1]=n_ba[(b)*amax   + (a+2)];
              ebuf[5]=n_ba[(b+1)*amax + (a+2)];
              ebuf[2]=n_ba[(b+2)*amax + (a+2)];
              ebuf[6]=n_ba[(b+2)*amax + (a+1)];
              ebuf[3]=n_ba[(b+2)*amax + a    ];
              ebuf[7]=n_ba[(b+1)*amax + a    ];
              elem_define( anz->emax+1, 10, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
            }

              /* RESTART of section mesh-improver, describe variables for the mesh-improver */
              for (ii=0; ii<8; ii++)
              {
                if( n_indx[e_enqire[anz->emax].nod[ii]]==-1) /* node not stored yet */
                {
                  n_ori[jj]=e_enqire[anz->emax].nod[ii];
                  n_coord[jj][0]=npre[n_ori[jj]].nx;
                  n_coord[jj][1]=npre[n_ori[jj]].ny;
                  n_coord[jj][2]=npre[n_ori[jj]].nz;
                  jj++;
                  n_indx[e_enqire[anz->emax].nod[ii]]=jj; /* first number is "1" to match the needs of the improver */
                }
#if TEST
		printf("en[i]:%d n_indx[en[i]]:%d k:%d i:%d\n", e_enqire[anz->emax].nod[ii],n_indx[e_enqire[anz->emax].nod[ii]],k,ii);
#endif
                e_nod[k][ii]=n_indx[e_enqire[anz->emax].nod[ii]];
              }
              /* INTERRUPT of section mesh-improver */

            k++;
            noelem10:;
          }
        }
      }
      
      surf[j].ne=k;

      /* RESTART of section mesh-improver: determine the edge nodes */
      if ((surf[j].etyp==9)||(surf[j].etyp==10))
      {
        v=0;
        u=0;
        a=0; 
        for (b=0; b<div_b; b++) { n_edge[v][u]=n_indx[n_ba[b*amax +a]]; if(n_edge[v][u]>=0) n_type[n_edge[v][u++]]=-1; } 
        s_div[v][0]=b;
        b=div_b; 
        for (a=0; a<div_a-offs_sa2; a++) { n_edge[v][u]=n_indx[n_ba[b*amax +a]]; if(n_edge[v][u]>=0) n_type[n_edge[v][u++]]=-1; } 
        s_div[v][1]=a;
        a=div_a; 
        for (b=div_b-offs_sa2; b>offs_sa1; b--) { n_edge[v][u]=n_indx[n_ba[b*amax +a]]; if(n_edge[v][u]>=0) n_type[n_edge[v][u++]]=-1; } 
        s_div[v][2]=div_b-offs_sa2-offs_sa1;
        b=0; 
        for (a=div_a-offs_sa1; a>0; a--) { n_edge[v][u]=n_indx[n_ba[b*amax +a]]; if(n_edge[v][u]>=0) n_type[n_edge[v][u++]]=-1; } 
        s_div[v][3]=div_a-offs_sa1;

        if (meshImprover( &surf[j].etyp, &jj, &k, n_indx, n_ori, n_coord, e_nod, n_type, n_edge, s_div )==0)
	{ 
          /* write the coordinates back */
          for (ii=0; ii<jj; ii++)
          {
            npre[n_ori[ii]].nx=n_coord[ii][0];
            npre[n_ori[ii]].ny=n_coord[ii][1];
            npre[n_ori[ii]].nz=n_coord[ii][2];
          }
	}
  
        /* free some space */
        for (ii=0; ii<((div_a+1)*(div_b+1)); ii++) free(n_coord[ii]);	
        for (ii=0; ii<((div_a+1)*(div_b+1)); ii++) free(e_nod[ii]); 
        for (ii=0; ii<MAX_SURFS_PER_BODY; ii++) free(n_edge[ii]);
        for (ii=0; ii<MAX_SURFS_PER_BODY; ii++) free(s_div[ii]);
      }
      /* END of section mesh-improver */

      if(( nurbsflag )&&( surf[j].sh>-1 ))
      {
        if(shape[surf[j].sh].type==4) projSurfToNurbs( nurbs, shape[surf[j].sh].p[0], surf, j, npre );
      }

      free(n_ba);
      free(n_uv);
      free(x);
      free(y);
      free(z);
    }
    else /* surf has balanced edges */
    {
      transitionflag=0;

      vmax=div_l[3]+1;
      umax=div_l[0]+1;

      if( (n_uv=(int *)malloc( (umax)*(vmax)*sizeof(int) ) )==NULL)
      { printf(" ERROR1: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[j].name);
        return(-1); }
      if( (x=(double *)malloc( (umax)*(vmax)*sizeof(double) ) )==NULL)
      { printf(" ERROR2: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[j].name);
        return(-1); }
      if( (y=(double *)malloc( (umax)*(vmax)*sizeof(double) ) )==NULL)
      { printf(" ERROR3: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[j].name);
        return(-1); }
      if( (z=(double *)malloc( (umax)*(vmax)*sizeof(double) ) )==NULL)
      { printf(" ERROR4: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[j].name);
        return(-1); }

      /* allocate memory for embeded nodes */
      if((surf[j].nod=(int *)realloc((int *)surf[j].nod, ((div_l[0]-1)*(div_l[3]-1)+1)*sizeof(int)) )==NULL)
      { printf(" ERROR5: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[j].name);
        return(-1); }

      /* knoten bestimmen  */
      if( fillSurf( j, n_uv, umax, vmax, x,y,z) <1) goto noEtypDefined;
      surf[j].fail=0;

      /* erzeugen der elemente   */
      k=0;

      /* allocate memory for final-node-buffer nbuf and final nodes */
      if ((nbuf = (int **)realloc((int **)nbuf, (apre->nmax+1)*sizeof(int *)) ) == NULL )
      { printf(" ERROR: realloc failure in meshSurf, nodes not installed\n\n"); return(-1); }
      for (v=sum_nbuf; v<=apre->nmax; v++)
      {
        if ((nbuf[v] = (int *)malloc( (2)*sizeof(int)) ) == NULL )
        { printf(" ERROR: realloc failure in meshSurf, nodes not installed\n\n"); return(-1); }
        nbuf[v][0]=0;
      }
      sum_nbuf=apre->nmax+1;

      if (fillSurfFlag)
      {
        surf[j].npgn=0;
        if( (surf[j].pgn=fptr= (GLdouble *)realloc( (GLdouble *)surf[j].pgn, (div_l[0]*div_l[3]*28)*sizeof(GLdouble) )) == NULL )
        { printf(" ERROR1: realloc failure in meshSurf()\n\n"); return(-1); }
        for (u=0; u<div_l[0]; u++)
        {
          for (v=0; v<div_l[3]; v++)
          {
            if (surf[j].ori=='+')
            {
              *fptr=GL_POLYGON_TOKEN; fptr++;
              *fptr=3; fptr++;
              v_result(&npre[n_uv[(u  )*vmax + v    ]].nx,&npre[n_uv[(u+1)*vmax + v    ]].nx, v1); 
              v_result(&npre[n_uv[(u  )*vmax + v    ]].nx,&npre[n_uv[(u  )*vmax + (v+1)]].nx, v2);
              v_prod(v1,v2,vn);
              v_norm(vn,fptr); fptr+=3;
              *fptr=npre[n_uv[(u  )*vmax + v    ]].nx; fptr++;
              *fptr=npre[n_uv[(u  )*vmax + v    ]].ny; fptr++;
              *fptr=npre[n_uv[(u  )*vmax + v    ]].nz; fptr++;
			                         
              *fptr=npre[n_uv[(u+1)*vmax + v    ]].nx; fptr++;
              *fptr=npre[n_uv[(u+1)*vmax + v    ]].ny; fptr++;
              *fptr=npre[n_uv[(u+1)*vmax + v    ]].nz; fptr++;
			                         
              *fptr=npre[n_uv[(u  )*vmax + (v+1)]].nx; fptr++;
              *fptr=npre[n_uv[(u  )*vmax + (v+1)]].ny; fptr++;
              *fptr=npre[n_uv[(u  )*vmax + (v+1)]].nz; fptr++;


              *fptr=GL_POLYGON_TOKEN; fptr++;
              *fptr=3; fptr++;
              v_result(&npre[n_uv[(u  )*vmax + (v+1)]].nx,&npre[n_uv[(u+1)*vmax + v    ]].nx, v1); 
              v_result(&npre[n_uv[(u  )*vmax + (v+1)]].nx,&npre[n_uv[(u+1)*vmax + (v+1)]].nx, v2);
              v_prod(v1,v2,vn);
              v_norm(vn,fptr); fptr+=3;
              *fptr=npre[n_uv[(u  )*vmax + (v+1)]].nx; fptr++;
              *fptr=npre[n_uv[(u  )*vmax + (v+1)]].ny; fptr++;
              *fptr=npre[n_uv[(u  )*vmax + (v+1)]].nz; fptr++;
			                         
              *fptr=npre[n_uv[(u+1)*vmax + v    ]].nx; fptr++;
              *fptr=npre[n_uv[(u+1)*vmax + v    ]].ny; fptr++;
              *fptr=npre[n_uv[(u+1)*vmax + v    ]].nz; fptr++;
			                         
              *fptr=npre[n_uv[(u+1)*vmax + (v+1)]].nx; fptr++;
              *fptr=npre[n_uv[(u+1)*vmax + (v+1)]].ny; fptr++;
              *fptr=npre[n_uv[(u+1)*vmax + (v+1)]].nz; fptr++;
              surf[j].npgn+=28;
            }
            else
            {
              *fptr=GL_POLYGON_TOKEN; fptr++;
              *fptr=3; fptr++;
              v_result(&npre[n_uv[(u  )*vmax + v    ]].nx,&npre[n_uv[(u  )*vmax + (v+1)]].nx, v1); 
              v_result(&npre[n_uv[(u  )*vmax + v    ]].nx,&npre[n_uv[(u+1)*vmax + v    ]].nx, v2);
              v_prod(v1,v2,vn);
              v_norm(vn,fptr); fptr+=3;
              *fptr=npre[n_uv[(u  )*vmax + v    ]].nx; fptr++;
              *fptr=npre[n_uv[(u  )*vmax + v    ]].ny; fptr++;
              *fptr=npre[n_uv[(u  )*vmax + v    ]].nz; fptr++;
			                         
              *fptr=npre[n_uv[(u  )*vmax + (v+1)]].nx; fptr++;
              *fptr=npre[n_uv[(u  )*vmax + (v+1)]].ny; fptr++;
              *fptr=npre[n_uv[(u  )*vmax + (v+1)]].nz; fptr++;
			                         
              *fptr=npre[n_uv[(u+1)*vmax + v    ]].nx; fptr++;
              *fptr=npre[n_uv[(u+1)*vmax + v    ]].ny; fptr++;
              *fptr=npre[n_uv[(u+1)*vmax + v    ]].nz; fptr++;


              *fptr=GL_POLYGON_TOKEN; fptr++;
              *fptr=3; fptr++;
              v_result(&npre[n_uv[(u  )*vmax + (v+1)]].nx,&npre[n_uv[(u+1)*vmax + (v+1)]].nx, v1); 
              v_result(&npre[n_uv[(u  )*vmax + (v+1)]].nx,&npre[n_uv[(u+1)*vmax + v    ]].nx, v2);
              v_prod(v1,v2,vn);
              v_norm(vn,fptr); fptr+=3;
              *fptr=npre[n_uv[(u  )*vmax + (v+1)]].nx; fptr++;
              *fptr=npre[n_uv[(u  )*vmax + (v+1)]].ny; fptr++;
              *fptr=npre[n_uv[(u  )*vmax + (v+1)]].nz; fptr++;
			                         
              *fptr=npre[n_uv[(u+1)*vmax + (v+1)]].nx; fptr++;
              *fptr=npre[n_uv[(u+1)*vmax + (v+1)]].ny; fptr++;
              *fptr=npre[n_uv[(u+1)*vmax + (v+1)]].nz; fptr++;
			                         
              *fptr=npre[n_uv[(u+1)*vmax + v    ]].nx; fptr++;
              *fptr=npre[n_uv[(u+1)*vmax + v    ]].ny; fptr++;
              *fptr=npre[n_uv[(u+1)*vmax + v    ]].nz; fptr++;
              surf[j].npgn+=28;
            }
          }
        }
      }
      else if (surf[j].etyp==7)
      {
        /* allocate memory for embeded elements */
        if((surf[j].elem=(int *)realloc((int *)surf[j].elem, (div_l[0]*div_l[3]*2)*sizeof(int)) )==NULL)
        { printf(" ERROR6: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[j].name);
          return(-1); }
        for (u=0; u<div_l[0]; u++)
        {
          for (v=0; v<div_l[3]; v++)
          {
            if (surf[j].ori=='+')
            {
              ebuf[0]=n_uv[(u  )*vmax + v    ];
              ebuf[1]=n_uv[(u+1)*vmax + v    ];
              ebuf[2]=n_uv[(u  )*vmax + (v+1)];
              elem_define( anz->emax+1, 7, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
              k++;
              ebuf[0]=n_uv[(u  )*vmax + (v+1)];
              ebuf[1]=n_uv[(u+1)*vmax + v    ];
              ebuf[2]=n_uv[(u+1)*vmax + (v+1)];
              elem_define( anz->emax+1, 7, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
            }
            else
	    {
              ebuf[0]=n_uv[(u  )*vmax + v    ];
              ebuf[1]=n_uv[(u  )*vmax + (v+1)];
              ebuf[2]=n_uv[(u+1)*vmax + v    ];
              elem_define( anz->emax+1, 7, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
              k++;
              ebuf[0]=n_uv[(u  )*vmax + (v+1)];
              ebuf[1]=n_uv[(u+1)*vmax + (v+1)];
              ebuf[2]=n_uv[(u+1)*vmax + v    ];
              elem_define( anz->emax+1, 7, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
            }
            k++;
          }
        }
      }
      else if (surf[j].etyp==8)
      {
        /* allocate memory for embeded elements */
        if((surf[j].elem=(int *)realloc((int *)surf[j].elem, (div_l[0]*div_l[3]/2)*sizeof(int)) )==NULL)
        { printf(" ERROR7: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[j].name);
          return(-1); }
        for (u=0; u<div_l[0]-1; u+=2)
        {
          for (v=0; v<div_l[3]-1; v+=2)
          {
            if (surf[j].ori=='+')
            {
              ebuf[0]=n_uv[(u  )*vmax + v    ];
              ebuf[1]=n_uv[(u+2)*vmax + v    ];
              ebuf[2]=n_uv[(u  )*vmax + (v+2)];
              ebuf[3]=n_uv[(u+1)*vmax + v    ];
              ebuf[4]=n_uv[(u+1)*vmax + (v+1)];
              ebuf[5]=n_uv[(u  )*vmax + (v+1)];
              elem_define( anz->emax+1, 8, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
              k++;
              ebuf[0]=n_uv[(u  )*vmax + (v+2)];
              ebuf[1]=n_uv[(u+2)*vmax + (v  )];
              ebuf[2]=n_uv[(u+2)*vmax + (v+2)];
              ebuf[3]=n_uv[(u+1)*vmax + (v+1)];
              ebuf[4]=n_uv[(u+2)*vmax + (v+1)];
              ebuf[5]=n_uv[(u+1)*vmax + (v+2)];
              elem_define( anz->emax+1, 8, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
            }
            else
	    {
              ebuf[0]=n_uv[(u  )*vmax + v    ];
              ebuf[1]=n_uv[(u  )*vmax + (v+2)];
              ebuf[2]=n_uv[(u+2)*vmax + v    ];
              ebuf[3]=n_uv[(u  )*vmax + (v+1)];
              ebuf[4]=n_uv[(u+1)*vmax + (v+1)];
              ebuf[5]=n_uv[(u+1)*vmax + (v  )];
              elem_define( anz->emax+1, 8, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
              k++;
              ebuf[0]=n_uv[(u  )*vmax + (v+2)];
              ebuf[1]=n_uv[(u+2)*vmax + (v+2)];
              ebuf[2]=n_uv[(u+2)*vmax + v    ];
              ebuf[3]=n_uv[(u+1)*vmax + (v+2)];
              ebuf[4]=n_uv[(u+2)*vmax + (v+1)];
              ebuf[5]=n_uv[(u+1)*vmax + (v+1)];
              elem_define( anz->emax+1, 8, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
            }
            k++;
          }
        }
      }
      else if (surf[j].etyp==9)
      {
        /* allocate memory for embeded elements */
        if((surf[j].elem=(int *)realloc((int *)surf[j].elem, (div_l[0]*div_l[3])*sizeof(int)) )==NULL)
        { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[j].name);
          return(-1); }

        if(writeCFDflag==1)
        {       
          /* store the node-indexes for block-structured cfd meshes */
          if(surf[j].ori=='+')
          {
            printf("surf:%s\n", surf[j].name);
            if ( (nBlock = (NodeBlocks *)realloc((NodeBlocks *)nBlock, (apre->b+1) * sizeof(NodeBlocks))) == NULL )
              printf("\n\n ERROR: realloc failed, NodeBlocks\n\n") ;
            if ( (nBlock[apre->b].nod = (int *)malloc( (umax*vmax+1) * sizeof(int))) == NULL )
              printf("\n\n ERROR: malloc failed, NodeBlocks\n\n") ;
            nBlock[apre->b].dim=2;
            nBlock[apre->b].geo=j;
            nBlock[apre->b].i=vmax;
            nBlock[apre->b].j=umax;
            nBlock[apre->b].k=1;
            n=0;
            for (u=0; u<umax; u++)
            {
              for (v=0; v<vmax; v++)
              {
                nBlock[apre->b].nod[n]=n_uv[u*vmax+v];
                n++;
    	      }
            }
  
            /* determine the connectivity for cfd-blocks */
            if(surf[j].nl!=4)
            {
              printf("PRG_ERROR: found surface with no 4 edges:%d, call the admin.\n",surf[j].nl);
              exit(1);
            }
            for (v=0; v<surf[j].nl; v++)
            {
              /* duns-edges are imin, imax, jmin, jmax, (kmin, kmax) */
              if(v==0) ii=0;
              else if(v==3) ii=1;
              else if(v==1) ii=2;
              else ii=3;
  
              nBlock[apre->b].neighbor[v]=-1;
              nBlock[apre->b].bcface[v]=-1;
              nBlock[apre->b].map[v][0]=-1;  nBlock[apre->b].map[v][1]=-1; nBlock[apre->b].map[v][2]=-1;
    	
              /* i==nBlock[].strt1[][0], j==nBlock[].strt1[][1] */
              imax= vmax; jmax= umax;
              if(ii==0)
              {
                nBlock[apre->b].strt1[v][0]=1;    nBlock[apre->b].strt1[v][1]=1;    nBlock[apre->b].strt1[v][2]=1; 
                nBlock[apre->b].end_1[v][0]=1;    nBlock[apre->b].end_1[v][1]=jmax; nBlock[apre->b].end_1[v][2]=2; 
              }
              if(ii==1)
              {
                nBlock[apre->b].strt1[v][0]=1;    nBlock[apre->b].strt1[v][1]=jmax; nBlock[apre->b].strt1[v][2]=1; 
                nBlock[apre->b].end_1[v][0]=imax; nBlock[apre->b].end_1[v][1]=jmax; nBlock[apre->b].end_1[v][2]=2; 
              }
              if(ii==2)
              {
                nBlock[apre->b].strt1[v][0]=imax; nBlock[apre->b].strt1[v][1]=jmax; nBlock[apre->b].strt1[v][2]=1; 
                nBlock[apre->b].end_1[v][0]=imax; nBlock[apre->b].end_1[v][1]=1;    nBlock[apre->b].end_1[v][2]=2; 
              }
              if(ii==3)
              {
                nBlock[apre->b].strt1[v][0]=imax; nBlock[apre->b].strt1[v][1]=1;    nBlock[apre->b].strt1[v][2]=1; 
                nBlock[apre->b].end_1[v][0]=1;    nBlock[apre->b].end_1[v][1]=1;    nBlock[apre->b].end_1[v][2]=2; 
              }
  
              surFlag=1;
              for (u=0; u<set[setNr].anz_s; u++)
              {
                if((u!=j)&&(surf[u].name != (char *)NULL))  for (jj=0; jj<surf[u].nl; jj++)
    	        {
                  if((surf[j].l[ii]==surf[u].l[jj])&&(surf[j].typ[ii]==surf[u].typ[jj]))
    	          {
                   if(surf[j].o[ii]!=surf[u].o[jj])
                   {
                    surFlag=0;
    
                    /* corresponding surface */
                    nBlock[apre->b].neighbor[v]=u;
  
                    if(surf[j].typ[ii]=='l') printf(" block:%d master-name:%s side[%d]:%s\n", j+1, surf[j].name, ii, line[surf[j].l[ii]].name );
                    if(surf[j].typ[ii]=='c') printf(" block:%d master-name:%s side[%d]:%s\n", j+1, surf[j].name, ii, line[surf[j].l[ii]].name );
  
                    if(surf[u].typ[jj]=='l') printf(" block:%d slave-name: %s side[%d]:%s\n", u+1, surf[u].name, jj, line[surf[u].l[jj]].name );
                    if(surf[u].typ[jj]=='c') printf(" block:%d slave-name: %s side[%d]:%s\n", u+1, surf[u].name, jj, line[surf[u].l[jj]].name );
  
                    if((ii==0)&&(jj==0))
                    {
                       nBlock[apre->b].map[v][0]=4; nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=3;
                    }
                    if((ii==1)&&(jj==0))
                    {
                      nBlock[apre->b].map[v][0]=5;  nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=3;
                    }
                    if((ii==2)&&(jj==0))
                    {
                      nBlock[apre->b].map[v][0]=1;  nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=3;
                    }
                    if((ii==3)&&(jj==0))
                    {
                      nBlock[apre->b].map[v][0]=2;  nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=3;
                    }
  
                    if((ii==0)&&(jj==1))
                    {
                      nBlock[apre->b].map[v][0]=2; nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=3; 
                    }
                    if((ii==1)&&(jj==1))
                    {
                      nBlock[apre->b].map[v][0]=4;  nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=3;
                    }
                    if((ii==2)&&(jj==1))
                    {
                      nBlock[apre->b].map[v][0]=5;  nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=3;
                    }
                    if((ii==3)&&(jj==1))
                    {
                      nBlock[apre->b].map[v][0]=1;  nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=3;
                    }
  
                    if((ii==0)&&(jj==2))
                    {
                      nBlock[apre->b].map[v][0]=1; nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=3; 
                    }
                    if((ii==1)&&(jj==2))
                    {
                      nBlock[apre->b].map[v][0]=2;  nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=3;
                    }
                    if((ii==2)&&(jj==2))
                    {
                      nBlock[apre->b].map[v][0]=4;  nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=3;
                    }
                    if((ii==3)&&(jj==2))
                    {
                      nBlock[apre->b].map[v][0]=5;  nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=3;
                    }
  
                    if((ii==0)&&(jj==3))
                    {
                      nBlock[apre->b].map[v][0]=5;  nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=3;
                    }
                    if((ii==1)&&(jj==3))
                    {
                      nBlock[apre->b].map[v][0]=1;  nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=3;
                    }
                    if((ii==2)&&(jj==3))
                    {
                      nBlock[apre->b].map[v][0]=2;  nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=3;
                    }
                    if((ii==3)&&(jj==3))
                    {
                      nBlock[apre->b].map[v][0]=4;  nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=3;
                    }
  
                    /* count the block-to-block interfaces for isaac */
                    apre->c++;
    
                    /* determine the amount of nodes in each direction of the surf */
                    if( surf[u].typ[3]=='c' )
                    {
                      imax=1;
                      for( l=0; l<lcmb[surf[u].l[3]].nl; l++ )
                      {
                        m=lcmb[surf[u].l[3]].l[l];
                        imax+=line[m].div;
                      }
                    }
                    else
                      imax=line[surf[u].l[3]].div+1;
  
                    if( surf[u].typ[0]=='c' )
                    {
                      jmax=1;
                      for( l=0; l<lcmb[surf[u].l[0]].nl; l++ )
                      {
                        m=lcmb[surf[u].l[0]].l[l];
                        jmax+=line[m].div;
                      }
                    }
                    else
                      jmax=line[surf[u].l[0]].div+1;
  
                    if(jj==0)
                    {
                      nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=1; 
                      nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=2; 
                    }
                    if(jj==1)
                    {
                      nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=1; 
                      nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=2; 
                    }
                    if(jj==2)
                    {
                      nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=1; 
                      nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=2; 
                    }
                    if(jj==3)
                    {
                      nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=1; 
                      nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=2; 
                    }
  
                   }
                   else
                   {
                     printf("ERROR surface orientation does not match, all surfs must be defined math-positive\n");
                   }
                  }
    	        }
      	      }
  
              if(surFlag)
              {
                /* found a free surface */
                nBlock[apre->b].neighbor[v]=anz_cfdSurfs+1;
                nBlock[apre->b].bcface[v]=ii;
                anz_cfdSurfs++;
  
                /* i==nBlock[].strt1[][0], j==nBlock[].strt1[][1], imin==1, imax==vmax, jmin==1, jmax==umax */
                /* i,j,k must be ascending, therefore they must be newly written! */
                if(ii==0)
                {
                  nBlock[apre->b].strt1[v][0]=1;    nBlock[apre->b].strt1[v][1]=1;    nBlock[apre->b].strt1[v][2]=1; 
                  nBlock[apre->b].end_1[v][0]=1;    nBlock[apre->b].end_1[v][1]=umax; nBlock[apre->b].end_1[v][2]=2; 
                }
                if(ii==1)
                {
                  nBlock[apre->b].strt1[v][0]=1;    nBlock[apre->b].strt1[v][1]=umax; nBlock[apre->b].strt1[v][2]=1; 
                  nBlock[apre->b].end_1[v][0]=vmax; nBlock[apre->b].end_1[v][1]=umax; nBlock[apre->b].end_1[v][2]=2; 
                }
                if(ii==2)
                {
                  /*
                  nBlock[apre->b].strt1[v][0]=vmax; nBlock[apre->b].strt1[v][1]=umax; nBlock[apre->b].strt1[v][2]=1; 
                  nBlock[apre->b].end_1[v][0]=vmax; nBlock[apre->b].end_1[v][1]=1;    nBlock[apre->b].end_1[v][2]=2; 
                  */
                  nBlock[apre->b].strt1[v][0]=vmax; nBlock[apre->b].strt1[v][1]=1;    nBlock[apre->b].strt1[v][2]=1; 
                  nBlock[apre->b].end_1[v][0]=vmax; nBlock[apre->b].end_1[v][1]=umax; nBlock[apre->b].end_1[v][2]=2; 
                }
                if(ii==3)
                {
                  /*
                  nBlock[apre->b].strt1[v][0]=vmax; nBlock[apre->b].strt1[v][1]=1;    nBlock[apre->b].strt1[v][2]=1; 
                  nBlock[apre->b].end_1[v][0]=1;    nBlock[apre->b].end_1[v][1]=1;    nBlock[apre->b].end_1[v][2]=2; 
                  */
                  nBlock[apre->b].strt1[v][0]=1;    nBlock[apre->b].strt1[v][1]=1;    nBlock[apre->b].strt1[v][2]=1; 
                  nBlock[apre->b].end_1[v][0]=vmax; nBlock[apre->b].end_1[v][1]=1;    nBlock[apre->b].end_1[v][2]=2; 
                }
    	      }
    	    }
  
            for (u=0; u<=div_l[0]; u++)
            {
              for (v=0; v<=div_l[3]; v++)
              {
                nod( anz, &node, 0, anz->nmax+1, npre[n_uv[u*vmax+v]].nx, npre[n_uv[u*vmax+v]].ny, npre[n_uv[u*vmax+v]].nz, 0 );
                if(nbuf[n_uv[u*vmax+v]][0]>0)
    	        {
                  if((nbuf[n_uv[u*vmax+v]] = (int *)realloc((int *)nbuf[n_uv[u*vmax+v]], (nbuf[n_uv[u*vmax+v]][0]+2)*sizeof(int)))==NULL )
                  { printf(" ERROR: realloc failure in meshSurf, nodes not installed\n\n"); return(-1); }
    	        }
                nbuf[n_uv[u*vmax+v]][0]++; nbuf[n_uv[u*vmax+v]][nbuf[n_uv[u*vmax+v]][0]]=anz->nmax;
    	      }
            }
  
            apre->b++;
          }
          else
  	  {
            printf("ERROR: surface:%s must be positive oriented but is negative oriented.\nAlso all surfaces must have a unique orientation\n", surf[j].name);
          }
	}
        /* end cfd */


        for (u=0; u<div_l[0]; u++)
        {
          for (v=0; v<div_l[3]; v++)
          {
            if (surf[j].ori=='+')
            {
              ebuf[0]=n_uv[(u  )*vmax + v    ];
              ebuf[1]=n_uv[(u+1)*vmax + v    ];
              ebuf[2]=n_uv[(u+1)*vmax + (v+1)];
              ebuf[3]=n_uv[(u  )*vmax + (v+1)];
              elem_define( anz->emax+1, 9, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
            }
            else
	    {
              ebuf[0]=n_uv[(u  )*vmax + v    ];
              ebuf[1]=n_uv[(u  )*vmax + (v+1)];
              ebuf[2]=n_uv[(u+1)*vmax + (v+1)];
              ebuf[3]=n_uv[(u+1)*vmax + v    ];
              elem_define( anz->emax+1, 9, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
            }
            k++;
          }
        }
      }
      else if (surf[j].etyp==10)
      {
        /* allocate memory for embeded elements */
        if((surf[j].elem=(int *)realloc((int *)surf[j].elem, (div_l[0]*div_l[3]/4)*sizeof(int)) )==NULL)
        { printf(" ERROR9: realloc failure in meshSurfs surf:%s can not be meshed\n\n", surf[j].name);
          return(-1); }
        for (u=0; u<div_l[0]; u+=2)
        {
          for (v=0; v<div_l[3]; v+=2)
          {
            if (surf[j].ori=='+')
            {
              ebuf[0]=n_uv[(u  )*vmax + v    ];
              ebuf[4]=n_uv[(u+1)*vmax +  v   ];
              ebuf[1]=n_uv[(u+2)*vmax +  v   ];
              ebuf[5]=n_uv[(u+2)*vmax + (v+1)];
              ebuf[2]=n_uv[(u+2)*vmax + (v+2)];
              ebuf[6]=n_uv[(u+1)*vmax + (v+2)];
              ebuf[3]=n_uv[(u)*vmax   + (v+2)];
              ebuf[7]=n_uv[(u  )*vmax + (v+1)];
              elem_define( anz->emax+1, 10, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
            }
            else
	    {
              ebuf[0]=n_uv[(u  )*vmax + v    ];
              ebuf[4]=n_uv[(u  )*vmax + (v+1)];
              ebuf[1]=n_uv[(u)*vmax   + (v+2)];
              ebuf[5]=n_uv[(u+1)*vmax + (v+2)];
              ebuf[2]=n_uv[(u+2)*vmax + (v+2)];
              ebuf[6]=n_uv[(u+2)*vmax + (v+1)];
              ebuf[3]=n_uv[(u+2)*vmax +  v   ];
              ebuf[7]=n_uv[(u+1)*vmax +  v   ];
              elem_define( anz->emax+1, 10, ebuf, 0, surf[j].eattr );
              surf[j].elem[k]=anz->emax;
            }
            k++;
          }
        }
      }
      
      surf[j].ne=k;

      if(( nurbsflag )&&( surf[j].sh>-1 )) 
      {
        if(shape[surf[j].sh].type==4) projSurfToNurbs( nurbs, shape[surf[j].sh].p[0], surf, j, npre );
      }

      free(n_uv);
      free(x);
      free(y);
      free(z);
    } /* end check of divisions */

    /* if a substitute surf was meshed then map the mesh onto the original one */
    if(mapflag)
    {
      s=set[setNr].surf[i];

      if(surf[j].ne>0)
      {
        if((surf[s].elem=(int *)realloc((int *)surf[s].elem, (surf[j].ne)*sizeof(int)) )==NULL)
        { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->elem (5)\n\n", surf[s].name); goto noMesh; }
        for(k=0; k<surf[j].ne; k++) surf[s].elem[k]=surf[j].elem[k];
        surf[s].ne=surf[j].ne;
      }
      if(surf[j].nn>0)
      {
        if((surf[s].nod=(int *)realloc((int *)surf[s].nod, (surf[j].nn)*sizeof(int)) )==NULL)
        { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->nod (2)\n\n", surf[s].name); goto noMesh; }
        for(k=0; k<surf[j].nn; k++) surf[s].nod[k]=surf[j].nod[k];
        surf[s].nn=surf[j].nn;
      }
      if(surf[j].npgn>0)
      {
        if((surf[s].pgn=(GLdouble *)realloc((GLdouble *)surf[s].pgn, (surf[j].npgn)*sizeof(GLdouble)) )==NULL)
        { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->nod (2)\n\n", surf[s].name); goto noMesh; }
        for(k=0; k<surf[j].npgn; k++) surf[s].pgn[k]=surf[j].pgn[k];
        surf[s].npgn=surf[j].npgn;
      }
      surf[s].fail=0;
    }

    j=set[setNr].surf[i];
    if((surf[j].etyp==7)&&(surf[j].ne>0)&&((mapflag)||(transitionflag)))
    {
      /* check the orientation of the first element which matches the first line of the surf */
      /* if it is inverse to the surface then change the orientation */
      /* of the surface temporarily */

      /* the first line in the surface definition defines the orientation */
      /* if the connected element is invers orientated then all elements are inverted */

      /* get the first two nodes of the surface-lines */
#if TEST
      printf("get nodes in surf:%s\n",surf[j].name);
#endif
      if(surf[j].typ[0]=='l')
      {
#if TEST
        printf("line:%s\n", line[surf[j].l[0]].name);
#endif
        if(surf[j].o[0]=='+')
        {
          nori[0]= point[line[surf[j].l[0]].p1].nod[0];
          if(line[surf[j].l[0]].nn>0) nori[1]= line[surf[j].l[0]].nod[0];
          else nori[1]= point[line[surf[j].l[0]].p2].nod[0];
        }
        else
        {
          nori[1]= point[line[surf[j].l[0]].p1].nod[0];
          if(line[surf[j].l[0]].nn>0) nori[0]= line[surf[j].l[0]].nod[0];
          else nori[0]= point[line[surf[j].l[0]].p2].nod[0];
        }
      }
      else
      {
#if TEST
        printf("lcmb:%s\n", lcmb[surf[j].l[0]].name);
#endif
        if(lcmb[surf[j].l[0]].o[0]=='+')
        {
          nori[0]= point[line[lcmb[surf[j].l[0]].l[0]].p1].nod[0];
          if(line[lcmb[surf[j].l[0]].l[0]].nn>0) nori[1]= line[lcmb[surf[j].l[0]].l[0]].nod[0];
          else nori[1]= point[line[lcmb[surf[j].l[0]].l[0]].p2].nod[0];
        }
        else
        {
          nori[1]= point[line[lcmb[surf[j].l[0]].l[0]].p1].nod[0];
          if(line[lcmb[surf[j].l[0]].l[0]].nn>0) nori[0]= line[lcmb[surf[j].l[0]].l[0]].nod[0];
          else nori[0]= point[line[lcmb[surf[j].l[0]].l[0]].p2].nod[0];
        }
        if(surf[j].o[0]=='-') { k=nori[0]; nori[0]=nori[1]; nori[1]=k; }
      }
#if TEST
      printf("nodes are :%d %d\n", nori[0],nori[1]);
#endif
 
      /* go over all elements and search the associated one */
      for(k=0; k<surf[j].ne; k++)
      {
#if TEST
        printf("check el:%d from:%d\n",surf[j].elem[k], surf[j].ne); 
#endif
        a=0;
        if      (e_enqire[surf[j].elem[k]].type == 1) ipuf = 8;   /* HEXA8  */
        else if (e_enqire[surf[j].elem[k]].type == 2) ipuf = 6;   /* PE6   */
        else if (e_enqire[surf[j].elem[k]].type == 3) ipuf = 4;   /* TET4   */
        else if (e_enqire[surf[j].elem[k]].type == 4) ipuf = 20;  /* HEXA20 */
        else if (e_enqire[surf[j].elem[k]].type == 5) ipuf = 15;  /* PE15  */
        else if (e_enqire[surf[j].elem[k]].type == 6) ipuf = 10;  /* TET10  */
        else if (e_enqire[surf[j].elem[k]].type == 7) ipuf = 3;   /* TRI3   */
        else if (e_enqire[surf[j].elem[k]].type == 8) ipuf = 6;   /* TRI6   */
        else if (e_enqire[surf[j].elem[k]].type == 9) ipuf = 4;   /* QUAD4  */
        else if (e_enqire[surf[j].elem[k]].type == 10) ipuf = 10; /* QUAD8  */
        else if (e_enqire[surf[j].elem[k]].type == 11) ipuf = 2;  /* BEAM2   */
        else if (e_enqire[surf[j].elem[k]].type == 12) ipuf = 3;  /* BEAM3   */

	/* create an array with the last node in front of all nodes */
	ebuf[0]=e_enqire[surf[j].elem[k]].nod[ipuf-1];
        for(ii=0; ii<ipuf; ii++) ebuf[ii+1]=e_enqire[surf[j].elem[k]].nod[ii];

        for(ii=0; ii<=ipuf; ii++)
        {
#if TEST
          printf("nod:%d\n", ebuf[ii]);
#endif
          for(n=0; n<2; n++)
          {
            if(nori[n]==ebuf[ii])
            {
              if(!a)
	      {
                b=ii;
                eori[a++]=n;
#if TEST
                printf("a:%d found n:%d nod:%d\n",a,n,nori[n]);
#endif
	      }
	      else if((a)&&(n!=eori[0])&&(b+1==ii))
	      {
                eori[a++]=n;
#if TEST
                printf("a:%d found n:%d nod:%d\n",a,n,nori[n]);
#endif
	      }
              else /* start again */
	      {
                b=ii;
                eori[0]=n;
#if TEST
                printf("a:%d found n:%d nod:%d\n",a,n,nori[n]);
#endif
	      }
            }
            if(a==2) goto found_reference;
          }
        }
      }
      printf(" ERROR: could not check orientation for surf:%s\n",surf[j].name);
      exit(-1);
      goto nextSurf;
    found_reference:;

      /* if the elements have to be reoriented */
      if(((eori[0]==1)&&(surf[j].ori=='+'))||((eori[0]==0)&&(surf[j].ori=='-')))
      {
#if TEST
        printf("rearrange:%s\n",surf[j].name); 
#endif
        for(k=0; k<surf[j].ne; k++)
        {
          for(ii=0; ii<ipuf; ii++) ebuf[ii]=e_enqire[surf[j].elem[k]].nod[ii];
          n=ii;
          for(ii=0; ii<ipuf; ii++)
          {
            e_enqire[surf[j].elem[k]].nod[ii]=ebuf[--n];
          }
        }
      }
    }

  checkSurf:;
    /* if the interiour of the surf was filled restore the original surface def */
    if(fillSurfFlag)
    {
      if(surfbuf[0].ne>0)
      {
        if((surf[j].elem=(int *)realloc((int *)surf[j].elem, (surfbuf[0].ne)*sizeof(int)) )==NULL)
        { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->elem (5)\n\n", surf[j].name); goto noMesh; }
        for(k=0; k<surfbuf[0].ne; k++) surf[j].elem[k]=surfbuf[0].elem[k];
        surf[j].ne=surfbuf[0].ne;
        free(surfbuf[0].elem);
      }
      if(surfbuf[0].nn>0)
      {
        if((surf[j].nod=(int *)realloc((int *)surf[j].nod, (surfbuf[0].nn)*sizeof(int)) )==NULL)
        { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->nod (2)\n\n", surf[j].name); goto noMesh; }
        for(k=0; k<surfbuf[0].nn; k++) surf[j].nod[k]=surfbuf[0].nod[k];
        surf[j].nn=surfbuf[0].nn;
        free(surfbuf[0].nod);
      }
    }
    goto nextSurf;
    noEtypDefined:;
    noSurfMesh++;
    pre_seta(specialset->nomesh, "s", surf[set[setNr].surf[i]].name);
    nextSurf:;
  }
  return(noSurfMesh);
 noMesh:;
  return(-1);
}


/**********************************************************************************/
/* Fuellt ein xyz- und nodeNr-feld mit den koordinaten eines Bodies bei der       */
/* gegenueber liegende seiten gleiche divisions haben.                            */
/* Die u-achse laeuft von bcp0 nach bcp4                                          */
/* Die v-achse laeuft von bcp0 nach bcp1                                          */
/* Die w-achse laeuft von bcp0 nach bcp3                                          */
/*                                                                                */
/* bestimmung der Referenzpunkte der Surfaces:                                    */
/* srefp[0]=cp-index der Bodysurf:0 der mit bcp[0] zussammenfaellt.               */
/* srefp[1]=cp-index der Bodysurf:1 der mit bcp[7] zussammenfaellt.               */
/* srefp[2]=cp-index der Bodysurf:2 der mit bcp[0] zussammenfaellt.               */
/* srefp[3]=cp-index der Bodysurf:3 der mit bcp[0] zussammenfaellt.               */
/* srefp[4]=cp-index der Bodysurf:4 der mit bcp[3] zussammenfaellt.               */
/* srefp[5]=cp-index der Bodysurf:5 der mit bcp[1] zussammenfaellt.               */
/*                                                                                */
/*                                                                                */
/* in:                                                                            */
/* b        body   -index                                                         */
/* srefp    surface-reference-points                                              */
/* umax, vmax, wmax  Anzahl nodes in den drei richtungen (div+1)                  */
/*                                                                                */
/* out:                                                                           */
/* n_uvw       alle nodeNrs im uvw-feld                                           */
/* x,y,z       =f(u,v,w) aus dem surfmesher, alle u,v,w positionen sind belegt    */
/*                                                                                */
/*                                                                                */
/**********************************************************************************/
int fillBody( int b, int *srefp, int umax, int vmax, int wmax, int *n_uvw, double *x, double *y, double *z )
{
  int s,j,n,k,l,m;
  int usmax, vsmax, us, vs, u,v,w, nodnr;
  double rv, rw, dx,dy,dz, xn,yn,zn;
  static int *div_l=NULL;
  static int *n_uv=NULL;  /* for edgenodes() */


  /* einlesen der Nodes der surf (1) in die Node-liste des Bodies  */
  /* surf1 liegt in der w-v ebene, u=umax-1  */
  s=1;
  u=umax-1;
  j=body[b].s[s];

  if( (n_uv=(int *)realloc((int *)n_uv, (umax)*(vmax)*(wmax)*sizeof(int)) ) == NULL )
  { printf(" ERROR: realloc failure in fillBody(), body:%s can not be meshed\n\n", body[b].name); return(-1); }
  if( (div_l=(int *)realloc((int *)div_l, (surf[j].nl+1)*sizeof(int)) ) == NULL )
  { printf(" ERROR: realloc failure in fillBody(), body:%s can not be meshed\n\n", body[b].name); return(-1); }
    
#if TEST
  fprintf (handle, "\nsur:%d %s\n", s, surf[j].name);
#endif
    
      if (surf[j].nl!=MAX_EDGES_PER_SURF)
      {
        printf(" ERROR: Surf has no %d edges (has:%d)\n", MAX_EDGES_PER_SURF, surf[j].nl );
        return(-1);
      }
      for (n=0; n<surf[j].nl; n++)
      {
        k=surf[j].l[n];
        div_l[n]=0;
        if( surf[j].typ[n]=='c' )
        {
          for( l=0; l<lcmb[k].nl; l++ )
          {
            m=lcmb[k].l[l];
            div_l[n]+=line[m].div;
          }
        }
        else
          div_l[n]+=line[k].div;
      }
      if ((div_l[0]!=div_l[2])||(div_l[1]!=div_l[3]))
      {
        printf(" ERROR: Surf:%s has unbalanced edges: %d %d %d %d\n", surf[j].name,
                div_l[0], div_l[1], div_l[2], div_l[3] );
        return(-1);
      }
      vsmax=div_l[3]+1;
      usmax=div_l[0]+1;
      edgeNodes( vsmax, usmax, j, n_uv );

      /* belege die Randknoten  */
      /* abhaengig von der orientierung und dem referenzpunkt der surface  */
      for (us=0; us<usmax; us++)
      {
        for (vs=0; vs<vsmax; vs++)
        {
          nodnr=n_uv[us*vsmax +vs];
          if (nodnr>-1)
	  {
            if (srefp[s]==0) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                w=usmax-us-1;
                v=vs;
              }
              else
              {
                w=vsmax-vs-1;
                v=us;
              }
            }
            else if (srefp[s]==1) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                w=vsmax-vs-1;
                v=usmax-us-1;
              }
              else
              {
                w=us;
                v=vs;
              }
            }
            else if (srefp[s]==2) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
		/*
                w=vsmax-vs-1;
                v=us;
		*/
                v=vsmax-vs-1;
                w=us;
              }
              else
              {
/*
                w=usmax-us-1;
                v=vs;
*/
                v=usmax-us-1;
                w=vs;
              }
            }
            else if (srefp[s]==3) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                w=vs;
                v=us;
              }
              else
              {
                w=usmax-us-1;
                v=vsmax-vs-1;
              }
            }
            else
            {
              errMsg(" ERROR: in meshBodies edge%d not known:%d\n",s, srefp[s]);
              exit(-1);
            }
          n_uvw[u*vmax*wmax +v*wmax +w]=nodnr;
            x[u*vmax*wmax +v*wmax +w]=npre[nodnr].nx;
            y[u*vmax*wmax +v*wmax +w]=npre[nodnr].ny;
            z[u*vmax*wmax +v*wmax +w]=npre[nodnr].nz;
#if TEST
  fprintf (handle, "n:%d %d %d %d\n", nodnr, u,v,w);
  sprintf( buffer, "%d ", nodnr);
  sprintf( namebuf, "bs%d ", s);
  pre_seta( namebuf, "n", buffer);
#endif
	    
	  }
        }
      }
      /* auffuellen der surface mit nodes */
      k=0;
      for (us=1; us<div_l[0]; us++)
      {
        for (vs=1; vs<div_l[3]; vs++)
        {
          nodnr=surf[j].nod[k];
            if (srefp[s]==0) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                w=usmax-us-1;
                v=vs;
              }
              else
              {
                w=vsmax-vs-1;
                v=us;
              }
            }
            else if (srefp[s]==1) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                w=vsmax-vs-1;
                v=usmax-us-1;
              }
              else
              {
                w=us;
                v=vs;
              }
            }
            else if (srefp[s]==2) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
		/*
                w=vsmax-vs-1;
                v=us;
                */
                v=vsmax-vs-1;
                w=us;
              }
              else
              {
/*
                w=usmax-us-1;
                v=vs;
*/
                v=usmax-us-1;
                w=vs;
              }
            }
            else if (srefp[s]==3) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                w=vs;
                v=us;
              }
              else
              {
                w=usmax-us-1;
                v=vsmax-vs-1;
              }
            }
          else
          {
              errMsg(" ERROR: in meshBodies edge%d not known:%d\n",s, srefp[s]);
            exit(-1);
          }
          n_uvw[u*vmax*wmax +v*wmax +w]=nodnr;
          x[u*vmax*wmax +v*wmax +w]=npre[nodnr].nx;
          y[u*vmax*wmax +v*wmax +w]=npre[nodnr].ny;
          z[u*vmax*wmax +v*wmax +w]=npre[nodnr].nz;
#if TEST
  fprintf (handle, "n:%d %d %d %d\n", nodnr, u,v,w);
  sprintf( buffer, "%d ", nodnr);
  sprintf( namebuf, "bs%d ", s);
  pre_seta( namebuf, "n", buffer);
#endif
	  
          k++;
        }
      }


    /* surf4 liegt in der u-v ebene, w=wmax-1  */
    s=4;
    w=wmax-1;
    j=body[b].s[s];

#if TEST
  fprintf (handle, "\nsur:%d %s\n", s, surf[j].name);
#endif
    
      if (surf[j].nl!=MAX_EDGES_PER_SURF)
      {
        printf(" ERROR: Surf has no %d edges (has:%d)\n", MAX_EDGES_PER_SURF, surf[j].nl );
        return(-1);
      }
      for (n=0; n<surf[j].nl; n++)
      {
        k=surf[j].l[n];
        div_l[n]=0;
        if( surf[j].typ[n]=='c' )
        {
          for( l=0; l<lcmb[k].nl; l++ )
          {
            m=lcmb[k].l[l];
            div_l[n]+=line[m].div;
          }
        }
        else
          div_l[n]+=line[k].div;
      }
      if ((div_l[0]!=div_l[2])||(div_l[1]!=div_l[3]))
      {
        printf(" ERROR: Surf:%s has unbalanced edges: %d %d %d %d\n", surf[j].name,
                div_l[0], div_l[1], div_l[2], div_l[3] );
        return(-1);
      }
      vsmax=div_l[3]+1;
      usmax=div_l[0]+1;
      edgeNodes( vsmax, usmax, j, n_uv );

      /* belege die Randknoten  */
      /* abhaengig von der orientierung und dem referenzpunkt der surface  */
      for (us=0; us<usmax; us++)
      {
        for (vs=0; vs<vsmax; vs++)
        {
          nodnr=n_uv[us*vsmax +vs];
          if (nodnr>-1)
	      {
            if (srefp[s]==0) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                v=vs;
                u=us;
              }
              else
              {
                v=us;
                u=vs;
              }
            }
            else if (srefp[s]==1) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=vs;
                v=usmax-us-1;
              }
              else
              {
                u=usmax-us-1;
                v=vs;
              }
            }
            else if (srefp[s]==2) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                v=vsmax-vs-1;
                u=usmax-us-1;
              }
              else
              {
                v=usmax-us-1;
                u=vsmax-vs-1;
              }
            }
            else if (srefp[s]==3) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=vsmax-vs-1;
                v=us;
              }
              else
              {
                u=us;
                v=vsmax-vs-1;
              }
            }
            else
            {
              errMsg(" ERROR: in meshBodies edge%d not known:%d\n",s, srefp[s]);
              exit(-1);
            }
          n_uvw[u*vmax*wmax +v*wmax +w]=nodnr;
            x[u*vmax*wmax +v*wmax +w]=npre[nodnr].nx;
            y[u*vmax*wmax +v*wmax +w]=npre[nodnr].ny;
            z[u*vmax*wmax +v*wmax +w]=npre[nodnr].nz;
	    
#if TEST
  fprintf (handle, "n:%d %d %d %d\n", nodnr, u,v,w);
  sprintf( buffer, "%d ", nodnr);
  sprintf( namebuf, "bs%d ", s);
  pre_seta( namebuf, "n", buffer);
#endif
	    
          }
        }
      }
      /* auff�llen der surface mit nodes */
      k=0;
      for (us=1; us<div_l[0]; us++)
      {
        for (vs=1; vs<div_l[3]; vs++)
        {
          nodnr=surf[j].nod[k];
          if (srefp[s]==0) /* edge der surf am refpunkt */
          {
            if (body[b].o[s]=='+')
            {
              v=vs;
              u=us;
            }
            else
            {
              v=us;
              u=vs;
            }
          }
            else if (srefp[s]==1) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=vs;
                v=usmax-us-1;
              }
              else
              {
                u=usmax-us-1;
                v=vs;
              }
            }
          else if (srefp[s]==2) /* edge der surf am refpunkt */
          {
            if (body[b].o[s]=='+')
            {
              v=vsmax-vs-1;
              u=usmax-us-1;
            }
            else
            {
              v=usmax-us-1;
              u=vsmax-vs-1;
            }
          }
            else if (srefp[s]==3) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=vsmax-vs-1;
                v=us;
              }
              else
              {
                u=us;
                v=vsmax-vs-1;
              }
            }
          else
          {
            errMsg(" ERROR: in meshBodies edge%d not known:%d\n",s, srefp[s]);
            exit(-1);
          }
          n_uvw[u*vmax*wmax +v*wmax +w]=nodnr;
          x[u*vmax*wmax +v*wmax +w]=npre[nodnr].nx;
          y[u*vmax*wmax +v*wmax +w]=npre[nodnr].ny;
          z[u*vmax*wmax +v*wmax +w]=npre[nodnr].nz;
	  
#if TEST
  fprintf (handle, "n:%d %d %d %d\n", nodnr, u,v,w);
  sprintf( buffer, "%d ", nodnr);
  sprintf( namebuf, "bs%d ", s);
  pre_seta( namebuf, "n", buffer);
#endif
	  
          k++;
        }
      }



    /* einlesen der Nodes der surf (2) in die Node-liste des Bodies  */
    /* surf2 liegt in der u-v ebene, w=0  */
    s=2;
    w=0;
    j=body[b].s[s];
    
#if TEST
  fprintf (handle, "\nsur:%d %s\n", s, surf[j].name);
#endif
    
      if (surf[j].nl!=MAX_EDGES_PER_SURF)
      {
        printf(" ERROR: Surf has no %d edges (has:%d)\n", MAX_EDGES_PER_SURF, surf[j].nl );
        return(-1);
      }
      for (n=0; n<surf[j].nl; n++)
      {
        k=surf[j].l[n];
        div_l[n]=0;
        if( surf[j].typ[n]=='c' )
        {
          for( l=0; l<lcmb[k].nl; l++ )
          {
            m=lcmb[k].l[l];
            div_l[n]+=line[m].div;
          }
        }
        else
          div_l[n]+=line[k].div;
      }
      if ((div_l[0]!=div_l[2])||(div_l[1]!=div_l[3]))
      {
        printf(" ERROR: Surf:%s has unbalanced edges: %d %d %d %d\n", surf[j].name,
                div_l[0], div_l[1], div_l[2], div_l[3] );
        return(-1);
      }
      vsmax=div_l[3]+1;
      usmax=div_l[0]+1;
      edgeNodes( vsmax, usmax, j, n_uv );

      /* belege die Randknoten  */
      /* abhaengig von der orientierung und dem referenzpunkt der surface  */
      for (us=0; us<usmax; us++)
      {
        for (vs=0; vs<vsmax; vs++)
        {
          nodnr=n_uv[us*vsmax +vs];
          if (nodnr>-1)
	      {
            if (srefp[s]==0) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=vs;
                v=us;
              }
              else
              {		
                u=us;
                v=vs;	
              }
            }
            else if (srefp[s]==1) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=usmax-us-1;
                v=vs;
              }
              else
              {
                u=vs;
                v=usmax-1-us;
              }
            }
            else if (srefp[s]==2) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=vsmax-1-vs;
                v=usmax-1-us;
              }
              else
              {
                u=usmax-1-us;
                v=vsmax-vs-1;
              }
            }
            else if (srefp[s]==3) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=us;
                v=vsmax-1-vs;
              }
              else
              {
                u=vsmax-vs-1;
                v=us;
              }
            }
            else
            {
              errMsg(" ERROR: in meshBodies edge%d not known:%d\n",s, srefp[s]);
              exit(-1);
            }
          n_uvw[u*vmax*wmax +v*wmax +w]=nodnr;
            x[u*vmax*wmax +v*wmax +w]=npre[nodnr].nx;
            y[u*vmax*wmax +v*wmax +w]=npre[nodnr].ny;
            z[u*vmax*wmax +v*wmax +w]=npre[nodnr].nz;
	    
#if TEST
  fprintf (handle, "n:%d %d %d %d\n", nodnr, u,v,w);
  sprintf( buffer, "%d ", nodnr);
  sprintf( namebuf, "bs%d ", s);
  pre_seta( namebuf, "n", buffer);
#endif
	    
	      }
        }
      }
      /* auffuellen der surface mit nodes */
      k=0;
      for (us=1; us<div_l[0]; us++)
      {
        for (vs=1; vs<div_l[3]; vs++)
        {
          nodnr=surf[j].nod[k];
            if (srefp[s]==0) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=vs;
                v=us;
              }
              else
              {		
                u=us;
                v=vs;	
              }
            }
            else if (srefp[s]==1) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=usmax-us-1;
                v=vs;
              }
              else
              {
                u=vs;
                v=usmax-1-us;
              }
            }
            else if (srefp[s]==2) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=vsmax-1-vs;
                v=usmax-1-us;
              }
              else
              {
                u=usmax-1-us;
                v=vsmax-vs-1;
              }
            }
            else if (srefp[s]==3) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=us;
                v=vsmax-1-vs;
              }
              else
              {
                u=vsmax-vs-1;
                v=us;
              }
            }
          else
          {
            errMsg(" ERROR: in meshBodies edge%d not known:%d\n",s, srefp[s]);
            exit(-1);
          }
          n_uvw[u*vmax*wmax +v*wmax +w]=nodnr;
          x[u*vmax*wmax +v*wmax +w]=npre[nodnr].nx;
          y[u*vmax*wmax +v*wmax +w]=npre[nodnr].ny;
          z[u*vmax*wmax +v*wmax +w]=npre[nodnr].nz;
	  
#if TEST
  fprintf (handle, "n:%d %d %d %d\n", nodnr, u,v,w);
  sprintf( buffer, "%d ", nodnr);
  sprintf( namebuf, "bs%d ", s);
  pre_seta( namebuf, "n", buffer);
#endif
	  
          k++;
        }
      }



    /* einlesen der Nodes der Mastersurf (0) in die Node-liste des Bodies  */
    /* surf0 liegt in der w-v ebene, u=0  */
    s=0;
    u=0;
    j=body[b].s[s];
    
#if TEST
  fprintf (handle, "\nsur:%d %s\n", s, surf[j].name);
#endif
    
      if (surf[j].nl!=MAX_EDGES_PER_SURF)
      {
        printf(" ERROR: Surf has no %d edges (has:%d)\n", MAX_EDGES_PER_SURF, surf[j].nl );
        return(-1);
      }
      for (n=0; n<surf[j].nl; n++)
      {
        k=surf[j].l[n];
        div_l[n]=0;
        if( surf[j].typ[n]=='c' )
        {
          for( l=0; l<lcmb[k].nl; l++ )
          {
            m=lcmb[k].l[l];
            div_l[n]+=line[m].div;
          }
        }
        else
          div_l[n]+=line[k].div;
      }
      if ((div_l[0]!=div_l[2])||(div_l[1]!=div_l[3]))
      {
        printf(" ERROR: Surf:%s has unbalanced edges: %d %d %d %d\n", surf[j].name,
                div_l[0], div_l[1], div_l[2], div_l[3] );
        return(-1);
      }
      vsmax=div_l[3]+1;
      usmax=div_l[0]+1;
      edgeNodes( vsmax, usmax, j, n_uv );

      /* belege die Randknoten  */
      /* abhaengig von der orientierung und dem referenzpunkt der surface  */
      for (us=0; us<usmax; us++)
      {
        for (vs=0; vs<vsmax; vs++)
        {
          nodnr=n_uv[us*vsmax +vs];
          if (nodnr>-1)
	      {
            if (srefp[s]==0) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                v=vs;
                w=us;
              }
              else
              {
                v=us;
                w=vs;
              }
            }
            else if (srefp[s]==1) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                w=vs;
                v=usmax-us-1;
              }
              else
              {
                w=usmax-us-1;
                v=vs;
              }
            }
            else if (srefp[s]==2) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                v=vsmax-vs-1;
                w=usmax-us-1;
              }
              else
              {
                v=usmax-us-1;
                w=vsmax-vs-1;
              }
            }
            else if (srefp[s]==3) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                w=vsmax-vs-1;
                v=us;
              }
              else
              {
                w=us;
                v=vsmax-vs-1;
              }
            }
            else
            {
              errMsg(" ERROR: in meshBodies edge%d not known:%d\n",s, srefp[s]);
              exit(-1);
            }
          n_uvw[u*vmax*wmax +v*wmax +w]=nodnr;
            x[u*vmax*wmax +v*wmax +w]=npre[nodnr].nx;
            y[u*vmax*wmax +v*wmax +w]=npre[nodnr].ny;
            z[u*vmax*wmax +v*wmax +w]=npre[nodnr].nz;
	    
#if TEST
  fprintf (handle, "n:%d %d %d %d\n", nodnr, u,v,w);
  sprintf( buffer, "%d ", nodnr);
  sprintf( namebuf, "bs%d ", s);
  pre_seta( namebuf, "n", buffer);
#endif
	    
	      }
        }
      }
      /* auff�llen der surface mit nodes */
      k=0;
      for (us=1; us<div_l[0]; us++)
      {
        for (vs=1; vs<div_l[3]; vs++)
        {
          nodnr=surf[j].nod[k];
          if (srefp[s]==0) /* edge der surf am refpunkt */
          {
            if (body[b].o[s]=='+')
            {
              v=vs;
              w=us;
            }
            else
            {
              v=us;
              w=vs;
            }
          }
            else if (srefp[s]==1) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                w=vs;
                v=usmax-us-1;
              }
              else
              {
                w=usmax-us-1;
                v=vs;
              }
            }
          else if (srefp[s]==2) /* edge der surf am refpunkt */
          {
            if (body[b].o[s]=='+')
            {
              v=vsmax-vs-1;
              w=usmax-us-1;
            }
            else
            {
              v=usmax-us-1;
              w=vsmax-vs-1;
            }
          }
            else if (srefp[s]==3) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                w=vsmax-vs-1;
                v=us;
              }
              else
              {
                w=us;
                v=vsmax-vs-1;
              }
            }
          else
          {
            errMsg(" ERROR: in meshBodies edge%d not known:%d\n",s, srefp[s]);
            exit(-1);
          }
          n_uvw[u*vmax*wmax +v*wmax +w]=nodnr;
          x[u*vmax*wmax +v*wmax +w]=npre[nodnr].nx;
          y[u*vmax*wmax +v*wmax +w]=npre[nodnr].ny;
          z[u*vmax*wmax +v*wmax +w]=npre[nodnr].nz;
	  
#if TEST
  fprintf (handle, "n:%d %d %d %d\n", nodnr, u,v,w);
  sprintf( buffer, "%d ", nodnr);
  sprintf( namebuf, "bs%d ", s);
  pre_seta( namebuf, "n", buffer);
#endif
          
          k++;
        }
      }


    /* surf3 liegt in der u-w ebene, v=0  */
    s=3;
    v=0;
    j=body[b].s[s];
    
#if TEST
  fprintf (handle, "\nsur:%d %s\n", s, surf[j].name);
#endif
    
      if (surf[j].nl!=MAX_EDGES_PER_SURF)
      {
        printf(" ERROR: Surf has no %d edges (has:%d)\n", MAX_EDGES_PER_SURF, surf[j].nl );
        return(-1);
      }
      for (n=0; n<surf[j].nl; n++)
      {
        k=surf[j].l[n];
        div_l[n]=0;
        if( surf[j].typ[n]=='c' )
        {
          for( l=0; l<lcmb[k].nl; l++ )
          {
            m=lcmb[k].l[l];
            div_l[n]+=line[m].div;
          }
        }
        else
          div_l[n]+=line[k].div;
      }
      if ((div_l[0]!=div_l[2])||(div_l[1]!=div_l[3]))
      {
        printf(" ERROR: Surf:%s has unbalanced edges: %d %d %d %d\n", surf[j].name,
                div_l[0], div_l[1], div_l[2], div_l[3] );
        return(-1);
      }
      vsmax=div_l[3]+1;
      usmax=div_l[0]+1;
      edgeNodes( vsmax, usmax, j, n_uv );

      /* belege die Randknoten  */
      /* abhaengig von der orientierung und dem referenzpunkt der surface  */
      for (us=0; us<usmax; us++)
      {
        for (vs=0; vs<vsmax; vs++)
        {
          nodnr=n_uv[us*vsmax +vs];
          if (nodnr>-1)
	      {
            if (srefp[s]==0) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                w=vs;
                u=us;
              }
              else
              {
                w=us;
                u=vs;
              }
            }
            else if (srefp[s]==1) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=vs;
                w=usmax-us-1;
              }
              else
              {
                u=usmax-us-1;
                w=vs;
              }
            }
            else if (srefp[s]==2) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                w=vsmax-vs-1;
                u=usmax-us-1;
              }
              else
              {
                w=usmax-us-1;
                u=vsmax-vs-1;
              }
            }
            else if (srefp[s]==3) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=vsmax-vs-1;
                w=us;
              }
              else
              {
                u=us;
                w=vsmax-vs-1;
              }
            }
            else
            {
              errMsg(" ERROR: in meshBodies edge%d not known:%d\n",s, srefp[s]);
              exit(-1);
            }
          n_uvw[u*vmax*wmax +v*wmax +w]=nodnr;
            x[u*vmax*wmax +v*wmax +w]=npre[nodnr].nx;
            y[u*vmax*wmax +v*wmax +w]=npre[nodnr].ny;
            z[u*vmax*wmax +v*wmax +w]=npre[nodnr].nz;
	    
#if TEST
  fprintf (handle, "n:%d %d %d %d\n", nodnr, u,v,w);
  sprintf( buffer, "%d ", nodnr);
  sprintf( namebuf, "bs%d ", s);
  pre_seta( namebuf, "n", buffer);
#endif
	    
	      }
        }
      }
      /* auff�llen der surface mit nodes */
      k=0;
      for (us=1; us<div_l[0]; us++)
      {
        for (vs=1; vs<div_l[3]; vs++)
        {
          nodnr=surf[j].nod[k];
          if (srefp[s]==0) /* edge der surf am refpunkt */
          {
            if (body[b].o[s]=='+')
            {
              w=vs;
              u=us;
            }
            else
            {
              w=us;
              u=vs;
            }
          }
            else if (srefp[s]==1) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=vs;
                w=usmax-us-1;
              }
              else
              {
                u=usmax-us-1;
                w=vs;
              }
            }
          else if (srefp[s]==2) /* edge der surf am refpunkt */
          {
            if (body[b].o[s]=='+')
            {
              w=vsmax-vs-1;
              u=usmax-us-1;
            }
            else
            {
              w=usmax-us-1;
              u=vsmax-vs-1;
            }
          }
            else if (srefp[s]==3) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=vsmax-vs-1;
                w=us;
              }
              else
              {
                u=us;
                w=vsmax-vs-1;
              }
            }
          else
          {
            errMsg(" ERROR: in meshBodies edge%d not known:%d\n",s, srefp[s]);
            exit(-1);
          }
          n_uvw[u*vmax*wmax +v*wmax +w]=nodnr;
          x[u*vmax*wmax +v*wmax +w]=npre[nodnr].nx;
          y[u*vmax*wmax +v*wmax +w]=npre[nodnr].ny;
          z[u*vmax*wmax +v*wmax +w]=npre[nodnr].nz;
	  
#if TEST
  fprintf (handle, "n:%d %d %d %d\n", nodnr, u,v,w);
  sprintf( buffer, "%d ", nodnr);
  sprintf( namebuf, "bs%d ", s);
  pre_seta( namebuf, "n", buffer);
#endif
	  
          k++;
        }
      }

    /* surf5 liegt in der w-u ebene, v=vmax-1  */
    s=5;
    v=vmax-1;
    j=body[b].s[s];
    
#if TEST
  fprintf (handle, "\nsur:%d %s\n", s, surf[j].name);
#endif
    
      if (surf[j].nl!=MAX_EDGES_PER_SURF)
      {
        printf(" ERROR: Surf has no %d edges (has:%d)\n", MAX_EDGES_PER_SURF, surf[j].nl );
        return(-1);
      }
      for (n=0; n<surf[j].nl; n++)
      {
        k=surf[j].l[n];
        div_l[n]=0;
        if( surf[j].typ[n]=='c' )
        {
          for( l=0; l<lcmb[k].nl; l++ )
          {
            m=lcmb[k].l[l];
            div_l[n]+=line[m].div;
          }
        }
        else
          div_l[n]+=line[k].div;
      }
      if ((div_l[0]!=div_l[2])||(div_l[1]!=div_l[3]))
      {
        printf(" ERROR: Surf:%s has unbalanced edges: %d %d %d %d\n", surf[j].name,
                div_l[0], div_l[1], div_l[2], div_l[3] );
        return(-1);
      }
      vsmax=div_l[3]+1;
      usmax=div_l[0]+1;
      edgeNodes( vsmax, usmax, j, n_uv );

      /* belege die Randknoten  */
      /* abhaengig von der orientierung und dem referenzpunkt der surface  */
      for (us=0; us<usmax; us++)
      {
        for (vs=0; vs<vsmax; vs++)
        {
          nodnr=n_uv[us*vsmax +vs];
          if (nodnr>-1)
	      {
            if (srefp[s]==0) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=vs;
                w=us;
              }
              else
              {
                u=us;
                w=vs;
              }
            }
            else if (srefp[s]==1) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                w=vs;
                u=usmax-us-1;
              }
              else
              {
                w=usmax-us-1;
                u=vs;
              }
            }
            else if (srefp[s]==2) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                u=vsmax-vs-1;
                w=usmax-us-1;
              }
              else
              {
                u=usmax-us-1;
                w=vsmax-vs-1;
              }
            }
            else if (srefp[s]==3) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                w=vsmax-vs-1;
                u=us;
              }
              else
              {
                w=us;
                u=vsmax-vs-1;
              }
            }
            else
            {
              errMsg(" ERROR: in meshBodies edge%d not known:%d\n",s, srefp[s]);
              exit(-1);
            }
          n_uvw[u*vmax*wmax +v*wmax +w]=nodnr;
            x[u*vmax*wmax +v*wmax +w]=npre[nodnr].nx;
            y[u*vmax*wmax +v*wmax +w]=npre[nodnr].ny;
            z[u*vmax*wmax +v*wmax +w]=npre[nodnr].nz;
	    
#if TEST
  fprintf (handle, "n:%d %d %d %d\n", nodnr, u,v,w);
  sprintf( buffer, "%d ", nodnr);
  sprintf( namebuf, "bs%d ", s);
  pre_seta( namebuf, "n", buffer);
#endif
	    
	      }
        }
      }
      /* auff�llen der surface mit nodes */
      k=0;
      for (us=1; us<div_l[0]; us++)
      {
        for (vs=1; vs<div_l[3]; vs++)
        {
          nodnr=surf[j].nod[k];
          if (srefp[s]==0) /* edge der surf am refpunkt */
          {
            if (body[b].o[s]=='+')
            {
              u=vs;
              w=us;
            }
            else
            {
              u=us;
              w=vs;
            }
          }
            else if (srefp[s]==1) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                w=vs;
                u=usmax-us-1;
              }
              else
              {
                w=usmax-us-1;
                u=vs;
              }
            }
          else if (srefp[s]==2) /* edge der surf am refpunkt */
          {
            if (body[b].o[s]=='+')
            {
              u=vsmax-vs-1;
              w=usmax-us-1;
            }
            else
            {
              u=usmax-us-1;
              w=vsmax-vs-1;
            }
          }
            else if (srefp[s]==3) /* edge der surf am refpunkt */
            {
              if (body[b].o[s]=='+')
              {
                w=vsmax-vs-1;
                u=us;
              }
              else
              {
                w=us;
                u=vsmax-vs-1;
              }
            }
          else
          {
            errMsg(" ERROR: in meshBodies edge%d not known:%d\n",s, srefp[s]);
            exit(-1);
          }
          n_uvw[u*vmax*wmax +v*wmax +w]=nodnr;
          x[u*vmax*wmax +v*wmax +w]=npre[nodnr].nx;
          y[u*vmax*wmax +v*wmax +w]=npre[nodnr].ny;
          z[u*vmax*wmax +v*wmax +w]=npre[nodnr].nz;
	  
#if TEST
  fprintf (handle, "n:%d %d %d %d\n", nodnr, u,v,w);
  sprintf( buffer, "%d ", nodnr);
  sprintf( namebuf, "bs%d ", s);
  pre_seta( namebuf, "n", buffer);
#endif
          
          k++;
        }
      }

    /* korrigieren der nodeposition der eingebetteten nodes fuer den kollabierten Body */
    s=1;
    u=umax-1;
    j=body[b].s[s];
    /* kollabierte seite feststellen, welcher seitenabstand ist 0? */
    dx= x[u*vmax*wmax +0*wmax +0]-x[u*vmax*wmax +(vmax-1)*wmax +0];
    dy= y[u*vmax*wmax +0*wmax +0]-y[u*vmax*wmax +(vmax-1)*wmax +0];
    dz= z[u*vmax*wmax +0*wmax +0]-z[u*vmax*wmax +(vmax-1)*wmax +0];
    rv =dx*dx + dy*dy + dz*dz;
    dx= x[u*vmax*wmax +0*wmax +0]-x[u*vmax*wmax +0*wmax +(wmax-1)];
    dy= y[u*vmax*wmax +0*wmax +0]-y[u*vmax*wmax +0*wmax +(wmax-1)];
    dz= z[u*vmax*wmax +0*wmax +0]-z[u*vmax*wmax +0*wmax +(wmax-1)];
    rw =dx*dx + dy*dy + dz*dz;
#if TEST
    printf("rv=%lf (if 0 colapsed) rw=%lf \n",rv,rw);
#endif
    if (rv==0.)
    {
      for (v=1; v<vmax-1; v++)
      {
        for (w=1; w<wmax-1; w++)
        {
          x[u*vmax*wmax +v*wmax +w]=x[u*vmax*wmax +0*wmax +w];
          y[u*vmax*wmax +v*wmax +w]=y[u*vmax*wmax +0*wmax +w];
          z[u*vmax*wmax +v*wmax +w]=z[u*vmax*wmax +0*wmax +w];
        }
      }
    }
    if (rw==0.)
    {
      for (v=1; v<vmax-1; v++)
      {
        for (w=1; w<wmax-1; w++)
        {
          x[u*vmax*wmax +v*wmax +w]=x[u*vmax*wmax +v*wmax +0];
          y[u*vmax*wmax +v*wmax +w]=y[u*vmax*wmax +v*wmax +0];
          z[u*vmax*wmax +v*wmax +w]=z[u*vmax*wmax +v*wmax +0];
        }
      }
    }

    /* auffuellen des bodies mit nodes */
    k=0;

    bodyMesh2( &wmax, &vmax, &umax, x, y, z);
    for (u=1; u<umax-1; u++)
    {
      for (v=1; v<vmax-1; v++)
      {
        for (w=1; w<wmax-1; w++)
        {
        xn=x[u*vmax*wmax +v*wmax +w];
        yn=y[u*vmax*wmax +v*wmax +w];
        zn=z[u*vmax*wmax +v*wmax +w];
        nod(  apre, &npre, 0, apre->nmax+1, xn, yn, zn, 0 );
        /* apre->nmax wird in nod um 1 erhoeht!  */
        n_uvw[u*vmax*wmax +v*wmax +w]=apre->nmax;
        body[b].nod[k]=apre->nmax;

#if TEST
  sprintf( buffer, "%d ", body[b].nod[k]);
  sprintf( namebuf, "bod%d ", b);
  pre_seta( namebuf, "n", buffer);
#endif

        k++;
        }
      }
    }
    body[b].nn=k;

  return(1);
}


/**********************************************************************************/
/*                                                                                */
/* bestimme die parametrischen Kantenlaengen der surf eines bodies                */
/*                                                                                */
/*                                                                                */
/* in:                                                                            */
/* b        body   -index                                                         */
/* s        surface-index in body-def                                             */
/*                                                                                */
/* out:                                                                           */
/* div_l    divisions of all surf-edges                                           */
/*                                                                                */
/* return:                                                                        */
/* unbalance  1: unbalanced edges, 0: balanced edges, -1 not meshable             */
/*                                                                                */
/**********************************************************************************/
int getSurfDivs( int b, int s, int *div_l )
{
  register int j, n, k, l, m;
  int sum_div;

  j=body[b].s[s];
  
  /* determine the amount of nodes in the surf without the nodes of the borderlines and points */
  /* => div.side1-1 * div.side2-1 = amount of embeded nodes */
  for (n=0; n<surf[j].nl; n++)
  {
    k=surf[j].l[n];
    div_l[n]=0;
    if( surf[j].typ[n]=='c' )
    {
      for( l=0; l<lcmb[k].nl; l++ )
      {
        m=lcmb[k].l[l];
        div_l[n]+=line[m].div;
      }
    }
    else
      div_l[n]+=line[k].div;
  }
  /* look if the divisions are suited for the elemtype  */
  /*  sum_div%2=0 for linear elements */
  sum_div=0;
  if (body[b].etyp==1)
  { 
    for (n=0; n<4; n++) sum_div+=div_l[n];
    if((sum_div&(int)1))
    {
      printf ("WARNING: bad divisions in body:%s\n", body[b].name);
      return(-1);
    }  
  }
  /* check each div%2=0 for quadratic elems, and check if the sum_div%4=0 */
  if (body[b].etyp==4)
  { 
    for (n=0; n<4; n++)
    { 
      if((div_l[n]&(int)1))
      {
        printf ("WARNING: bad divisions in body:%s\n", body[b].name);
        return(-1);
      } 
      sum_div+=div_l[n];
    } 
    if( ((sum_div&(int)1))||((sum_div&(int)2)) )
    {
      printf ("WARNING: bad divisions in body:%s\n", body[b].name);
      return(-1);
    }  
  }
  if ((div_l[0]!=div_l[2])||(div_l[1]!=div_l[3])) return(1);
  else return(0);
}


/**********************************************************************************/
/*                                                                                */
/* calculates new positions for existing nodes to gain a better quality           */
/*                                                                                */
/*  return 1 if bad elements exist else 0                                         */
/*                                                                                */
/**********************************************************************************/
int meshImprover( int *etyp, int *nodes, int *elements, int *n_indx, int *n_ori, double **n_coord, int **e_nod, int *n_type, int **n_edge, int **s_div )
{
  int i,j,k,n, nn[6], nr_surfs;

  Summen   anzl[1];
  Nodes    nodel[20]; 
  Elements eleml[1];

  /* allocate workspace: */
  /*  static int iptr1[nk], iptr2[6*nk], nside[nk], ineino[12*nk]; */
  /*  pneigh[3*maxsumdiv] */

  /* workspace for the improver */

  int    *kon=NULL, *neigh=NULL, *iptr1=NULL, *iptr2=NULL, *nside=NULL, *ineino=NULL, *nkind=NULL;
  double *co=NULL, *pneigh=NULL; 
  static int ndiv[24], maxsumdiv;
  double xl[20][3];
  char elty[MAX_LINE_LENGTH];
  int checkFailed=0;


  if((kon=(int *)malloc( (int)(*elements*20)*sizeof(int)) )==NULL)
  { printf(" ERROR: kon malloc failure in meshImproverHe20\n\n"); return(-1); }
  if((iptr1=(int *)malloc( (int)*nodes*sizeof(int)) )==NULL)
  { printf(" ERROR: iptr1 malloc failure in meshImproverHe20\n\n"); return(-1); }
  if((iptr2=(int *)malloc( (int)(*nodes*6)*sizeof(int)) )==NULL)
  { printf(" ERROR: iptr2 malloc failure in meshImproverHe20\n\n"); return(-1); }
  if((nside=(int *)malloc( (int)(*nodes)*sizeof(int)) )==NULL)
  { printf(" ERROR: nside malloc failure in meshImproverHe20\n\n"); return(-1); }
  if((ineino=(int *)malloc( (int)(*nodes*12)*sizeof(int)) )==NULL)
  { printf(" ERROR: ineino malloc failure in meshImproverHe20\n\n"); return(-1); }
  if((nkind=(int *)malloc( (int)(*nodes)*sizeof(int)) )==NULL)
  { printf(" ERROR: nkind malloc failure in meshImproverHe20\n\n"); return(-1); }

  if((pneigh=(double *)malloc( (int)(*nodes*3)*sizeof(double)) )==NULL)
  { printf(" ERROR: pneigh malloc failure in meshImproverHe20\n\n"); return(-1); }
  if((co=(double *)malloc( (int)(*nodes*3)*sizeof(double)) )==NULL)
  { printf(" ERROR: co malloc failure in meshImproverHe20\n\n"); return(-1); }

  for (i=0; i<*nodes; i++)
  {
    nkind[i]=n_type[i+1];
  }

  for (i=0; i<*nodes; i++)
    for (j=0; j<3; j++)
      co[i*3+j]=(double)n_coord[i][j];

  if(*etyp==1)
  {
    nr_surfs=6;
    for (i=0; i<*elements; i++)
    {
      for (j=0; j<8; j++)
        kon[i*20+j]=e_nod[i][j];
      for (j=8; j<20; j++)
        kon[i*20+j]=0;
    }
  }
  else if(*etyp==4)
  {
    nr_surfs=6;
    for (i=0; i<*elements; i++)
    {
      for (j=0; j<12; j++)
        kon[i*20+j]=e_nod[i][j];
      for (j=12; j<16; j++)
        kon[i*20+j+4]=e_nod[i][j];
      for (j=16; j<20; j++)
        kon[i*20+j-4]=e_nod[i][j];
    }
  }
  else if(*etyp==9)
  {
    nr_surfs=1;
    for (i=0; i<*elements; i++)
    {
      for (j=0; j<4; j++)
        kon[i*20+j]=e_nod[i][j];
      for (j=4; j<20; j++)
        kon[i*20+j]=0;
    }
  }
  else if(*etyp==10)
  {
    nr_surfs=1;
    for (i=0; i<*elements; i++)
    {
      for (j=0; j<8; j++)
        kon[i*20+j]=e_nod[i][j];
      for (j=8; j<20; j++)
        kon[i*20+j]=0;
    }
  }
  else return(-1);

  maxsumdiv=0;
  for (i=0; i<nr_surfs; i++)
  {
    nn[i]=0;
    for (j=0; j<4; j++) { nn[i]+=s_div[i][j]; }
    if(nn[i]>maxsumdiv) maxsumdiv=nn[i];
  }
  if((neigh=(int *)malloc( (int)(maxsumdiv*12)*sizeof(int)) )==NULL)
  { printf(" ERROR: neigh malloc failure in meshImproverHe20\n\n"); return(-1); }
  for (i=0; i<nr_surfs; i++)
    for (j=0; j<nn[i]; j++) 
      neigh[i*maxsumdiv+j]=n_edge[i][j];


  for (i=0; i<nr_surfs; i++)
    for (j=0; j<4; j++)
    {
      ndiv[i*4+j]=s_div[i][j];
    }


  if ((meshopt_length)||(oldmeshflag))
    smooth_length( co,nodes,kon,elements,iptr1,iptr2,nside,ineino,nkind,neigh,ndiv,pneigh,&maxsumdiv,etyp, n_ori);
  if (oldmeshflag) goto finish;
  if (meshopt_angle)
    smooth_angle( co,nodes,kon,elements,iptr1,iptr2,nside,ineino,nkind,neigh,ndiv,pneigh,&maxsumdiv,etyp, n_ori);

  /* check the mesh for negative jacobian matrix */
  anzl->e=1;
  checkFailed=0;
  for (i=0; i<*elements; i++)
  {
    if(*etyp == 1)
    {
      eleml[0].type=1;
      for(j=0; j<8; j++)
      {
        nodel[j].nx=co[3*(e_nod[i][j]-1)+0];
        nodel[j].ny=co[3*(e_nod[i][j]-1)+1];
        nodel[j].nz=co[3*(e_nod[i][j]-1)+2];
        eleml[0].nod[j]=j;
      }
      elemChecker( anzl->e, nodel, eleml);
      for(j=0; j<8; j++)
      { 
        co[3*(e_nod[i][j]-1)+0]=nodel[j].nx;
        co[3*(e_nod[i][j]-1)+1]=nodel[j].ny;
        co[3*(e_nod[i][j]-1)+2]=nodel[j].nz;
      }
      for(j=0; j<8; j++)
      { 
        for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][j]-1)+k];
      }
      strcpy(elty,"C3D8");
      if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
      strcpy(elty,"C3D8R");
      if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
    }
    else if(*etyp == 4)
    {
      eleml[0].type=4;
      for(j=0; j<20; j++)
      {
        nodel[j].nx=co[3*(e_nod[i][j]-1)+0];
        nodel[j].ny=co[3*(e_nod[i][j]-1)+1];
        nodel[j].nz=co[3*(e_nod[i][j]-1)+2];
        eleml[0].nod[j]=j;
      }
      elemChecker( anzl->e, nodel, eleml);
      for(j=0; j<20; j++)
      { 
        co[3*(e_nod[i][j]-1)+0]=nodel[j].nx;
        co[3*(e_nod[i][j]-1)+1]=nodel[j].ny;
        co[3*(e_nod[i][j]-1)+2]=nodel[j].nz;
      }

      for(j=0; j<12; j++)
      { 
        for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][j]-1)+k];
      }
      for(n=16; n<20; n++)
      { 
        for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][n]-1)+k];
        j++;
      }
      for(n=12; n<16; n++)
      { 
        for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][n]-1)+k];
        j++;
      }
      strcpy(elty,"C3D20");
      if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
      strcpy(elty,"C3D20R");
      if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
    }
    else if(*etyp == 6)
    {
      for(j=0; j<10; j++)
      { 
        for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][j]-1)+k];
      }
      strcpy(elty,"C3D10");
      if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
    }
  }

  /* if negative jacobian matrix do smooth_length alone */
  if ((meshopt_length)&&(checkFailed))
  {
    printf ("WARNING: smooth_line only\n");
    smooth_length( co,nodes,kon,elements,iptr1,iptr2,nside,ineino,nkind,neigh,ndiv,pneigh,&maxsumdiv,etyp, n_ori);
  }

  if (checkFailed)
  {
    /* check the mesh for negative jacobian matrix */
    checkFailed=0;
    for (i=0; i<*elements; i++)
    {
      if(*etyp == 1)
      {
        eleml[0].type=1;
        for(j=0; j<8; j++)
        {
          nodel[j].nx=co[3*(e_nod[i][j]-1)+0];
          nodel[j].ny=co[3*(e_nod[i][j]-1)+1];
          nodel[j].nz=co[3*(e_nod[i][j]-1)+2];
          eleml[0].nod[j]=j;
        }
        elemChecker( anzl->e, nodel, eleml);
        for(j=0; j<8; j++)
        { 
          co[3*(e_nod[i][j]-1)+0]=nodel[j].nx;
          co[3*(e_nod[i][j]-1)+1]=nodel[j].ny;
          co[3*(e_nod[i][j]-1)+2]=nodel[j].nz;
        }

        for(j=0; j<8; j++)
        { 
          for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][j]-1)+k];
        }
        strcpy(elty,"C3D8");
        if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
        strcpy(elty,"C3D8R");
        if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
      }
      else if(*etyp == 4)
      {
        eleml[0].type=4;
        for(j=0; j<20; j++)
        {
          nodel[j].nx=co[3*(e_nod[i][j]-1)+0];
          nodel[j].ny=co[3*(e_nod[i][j]-1)+1];
          nodel[j].nz=co[3*(e_nod[i][j]-1)+2];
          eleml[0].nod[j]=j;
        }
        elemChecker( anzl->e, nodel, eleml);
        for(j=0; j<20; j++)
        { 
          co[3*(e_nod[i][j]-1)+0]=nodel[j].nx;
          co[3*(e_nod[i][j]-1)+1]=nodel[j].ny;
          co[3*(e_nod[i][j]-1)+2]=nodel[j].nz;
        }

        for(j=0; j<12; j++)
        { 
          for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][j]-1)+k];
        }
        for(n=16; n<20; n++)
        { 
          for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][n]-1)+k];
          j++;
        }
        for(n=12; n<16; n++)
        { 
          for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][n]-1)+k];
          j++;
        }
        strcpy(elty,"C3D20");
        if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
        strcpy(elty,"C3D20R");
        if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
      }
      else if(*etyp == 6)
      {
        for(j=0; j<10; j++)
        { 
          for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][j]-1)+k];
        }
        strcpy(elty,"C3D10");
        if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
      }
    }
  }

  /* if negative jacobian matrix do smooth_angle alone */
  if ((meshopt_angle)&&(checkFailed))
  {
    printf ("WARNING: smooth_angle only\n");
    smooth_angle( co,nodes,kon,elements,iptr1,iptr2,nside,ineino,nkind,neigh,ndiv,pneigh,&maxsumdiv,etyp, n_ori);
  }

  if (checkFailed)
  {
    /* check the mesh for negative jacobian matrix */
    checkFailed=0;
    for (i=0; i<*elements; i++)
    {
      if(*etyp == 1)
      {
        eleml[0].type=1;
        for(j=0; j<8; j++)
        {
          nodel[j].nx=co[3*(e_nod[i][j]-1)+0];
          nodel[j].ny=co[3*(e_nod[i][j]-1)+1];
          nodel[j].nz=co[3*(e_nod[i][j]-1)+2];
          eleml[0].nod[j]=j;
        }
        elemChecker( anzl->e, nodel, eleml);
        for(j=0; j<8; j++)
        { 
          co[3*(e_nod[i][j]-1)+0]=nodel[j].nx;
          co[3*(e_nod[i][j]-1)+1]=nodel[j].ny;
          co[3*(e_nod[i][j]-1)+2]=nodel[j].nz;
        }

        for(j=0; j<8; j++)
        { 
          for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][j]-1)+k];
        }
        strcpy(elty,"C3D8");
        if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
        strcpy(elty,"C3D8R");
        if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
      }
      else if(*etyp == 4)
      {
        eleml[0].type=4;
        for(j=0; j<20; j++)
        {
          nodel[j].nx=co[3*(e_nod[i][j]-1)+0];
          nodel[j].ny=co[3*(e_nod[i][j]-1)+1];
          nodel[j].nz=co[3*(e_nod[i][j]-1)+2];
          eleml[0].nod[j]=j;
        }
        elemChecker( anzl->e, nodel, eleml);
        for(j=0; j<20; j++)
        { 
          co[3*(e_nod[i][j]-1)+0]=nodel[j].nx;
          co[3*(e_nod[i][j]-1)+1]=nodel[j].ny;
          co[3*(e_nod[i][j]-1)+2]=nodel[j].nz;
        }

        for(j=0; j<12; j++)
        { 
          for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][j]-1)+k];
        }
        for(n=16; n<20; n++)
        { 
          for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][n]-1)+k];
          j++;
        }
        for(n=12; n<16; n++)
        { 
          for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][n]-1)+k];
          j++;
        }
        strcpy(elty,"C3D20");
        if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
        strcpy(elty,"C3D20R");
        if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
      }
      else if(*etyp == 6)
      {
        for(j=0; j<10; j++)
        { 
          for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][j]-1)+k];
        }
        strcpy(elty,"C3D10");
        if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
      }
    }
  }

  /* if negative jacobian matrix do smooth_midnodes alone */
  if ((checkFailed) || (!meshopt_angle && !meshopt_length))
  {
    for (i=0; i<*nodes; i++)
      for (j=0; j<3; j++)
        co[i*3+j]=(double)n_coord[i][j];
    printf ("WARNING: no smoothing\n");
    smooth_midnodes( co,nodes,kon,elements,iptr1,iptr2,nside,ineino,nkind,neigh,ndiv,pneigh,&maxsumdiv,etyp, n_ori);
  }

  if (checkFailed)
  {
    /* check the mesh for negative jacobian matrix */
    checkFailed=0;
    for (i=0; i<*elements; i++)
    {
      if(*etyp == 1)
      {
        eleml[0].type=1;
        for(j=0; j<8; j++)
        {
          nodel[j].nx=co[3*(e_nod[i][j]-1)+0];
          nodel[j].ny=co[3*(e_nod[i][j]-1)+1];
          nodel[j].nz=co[3*(e_nod[i][j]-1)+2];
          eleml[0].nod[j]=j;
        }
        elemChecker( anzl->e, nodel, eleml);
        for(j=0; j<8; j++)
        { 
          co[3*(e_nod[i][j]-1)+0]=nodel[j].nx;
          co[3*(e_nod[i][j]-1)+1]=nodel[j].ny;
          co[3*(e_nod[i][j]-1)+2]=nodel[j].nz;
        }

        for(j=0; j<8; j++)
        { 
          for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][j]-1)+k];
        }
        strcpy(elty,"C3D8");
        if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
        strcpy(elty,"C3D8R");
        if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
      }
      else if(*etyp == 4)
      {
        eleml[0].type=4;
        for(j=0; j<20; j++)
        {
          nodel[j].nx=co[3*(e_nod[i][j]-1)+0];
          nodel[j].ny=co[3*(e_nod[i][j]-1)+1];
          nodel[j].nz=co[3*(e_nod[i][j]-1)+2];
          eleml[0].nod[j]=j;
        }
        elemChecker( anzl->e, nodel, eleml);
        for(j=0; j<20; j++)
        { 
          co[3*(e_nod[i][j]-1)+0]=nodel[j].nx;
          co[3*(e_nod[i][j]-1)+1]=nodel[j].ny;
          co[3*(e_nod[i][j]-1)+2]=nodel[j].nz;
        }

        for(j=0; j<12; j++)
        { 
          for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][j]-1)+k];
        }
        for(n=16; n<20; n++)
        { 
          for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][n]-1)+k];
          j++;
        }
        for(n=12; n<16; n++)
        { 
          for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][n]-1)+k];
          j++;
        }
        strcpy(elty,"C3D20");
        if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
        strcpy(elty,"C3D20R");
        if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
      }
      else if(*etyp == 6)
      {
        for(j=0; j<10; j++)
        { 
          for(k=0; k<3; k++) xl[j][k]= co[3*(e_nod[i][j]-1)+k];
        }
        strcpy(elty,"C3D10");
        if (!e_c3d__(&xl[0][0], elty)) checkFailed=1;
      }
    }
  }

 finish:;
  for (i=0; i<*nodes; i++)
  {
    for (j=0; j<3; j++)
    {
      n_coord[i][j]=co[i*3+j];
    }
  }
  free(kon);
  free(neigh);
  free(iptr1);
  free(iptr2);
  free(nside);
  free(ineino);
  free(nkind);
  free(co);
  free(pneigh);

  return(checkFailed);
} 




int bodyFrom5Surfs( int *b)
{
  int i,ii,j,jj,k,l,n,s, **sn=NULL, u,v, umax,vmax;
  int bs;          /* body-surf */
  int lm[2];       /* master-lines which are replaced by lines ls */
  int tm[2];       /* master-line-type which is replaced by lines ls */
  int ss[6];       /* slave surfs */
  int ps[2];       /* split points */
  int ls[2][2];    /* lines ls[surf][l] in the place of the master-lines */
  int ts[2][2];    /* type of lines ls[surf][l] in the place of the master-lines */
  int sm[2];       /* surface between 0 and 1 which is to be replaced by two new ones */
  int smi[2];      /* index in the body-topology surfaces sm[] */
  int lm2[2];      /* master-lines which are located between surf 0 and 1 */ 
  int tm2[2];      /* master-lines-type which are located between surf 0 and 1 */ 
  int om2[2];      /* master-lines-ori which are located between surf 0 and 1 */ 
  int lnew;        /* line between split-points */

  int clin[MAX_EDGES_PER_SURF], l1, l3;
  int edge[MAX_EDGES_PER_SURF];
  char ctyp[MAX_EDGES_PER_SURF]; 
  char cori[MAX_SURFS_PER_BODY];  /* orientation, used for slave surfs and body */

  int lnew_[2];
  char typnew[2];

  char name[MAX_LINE_LENGTH];


  /* store the basic info from the substitute surfs */
  for(bs=0; bs<2; bs++)
  {
    s=body[*b].s[bs];
    lm[bs]=surf[s].l[surf[s].l[3]];
    tm[bs]=surf[s].typ[surf[s].l[3]];
    ss[bs]=surf[s].l[4];
    ps[bs]=surf[s].l[5];
    ls[bs][0]=surf[s].l[6];
    ts[bs][0]=surf[s].l[7];
    ls[bs][1]=surf[s].l[8];
    ts[bs][1]=surf[s].l[9];

    /* search the surf sm (no. 2,3,4) which is related to the replaced line lm */
    for(i=2; i<5; i++)
    {
      sm[bs]=body[*b].s[i];
      smi[bs]=i;
      for(j=0; j<4; j++) if((surf[sm[bs]].l[j]==lm[bs])&&(surf[sm[bs]].typ[j]==tm[bs])) goto sm_found;
    }
    sm_found:;
  }

  /* this surfaces should be identical if the geometry was sweped */
  if(sm[0]!=sm[1])
  {
    /* the embedded surface ss[1] must be redefined. */
    printf("WARNING: Untested code in bodyFrom5Surfs()\n");

    /* search the opposide line to lm[0] on surf sm[1]. This is also part of surf =body[*b].s[1] */
    printf("the embedded surface ss[1] must be redefined lm[0]:%s lm[1]:%s\n", line[lm[0]].name, line[lm[1]].name);
    lm[1]=-1;
    s=body[*b].s[1];
    for(i=0; i<3; i++)
      for(j=0; j<4; j++)
        if((surf[sm[1]].l[j]==surf[s].l[i])&&(surf[sm[1]].typ[j]==surf[s].typ[i]))
        { lm[1]=surf[s].l[i]; goto foundLine; }
    printf("ERROR: line opposide to lm[0] not found\n");
   foundLine:;
    printf("new lm[1]:%s\n", line[lm[1]].name);
    if(lm[1]==-1) { printf(" ERROR: 5-sided body not meshable\n"); return(-1); }

    ps[1]=splitLineAtDivratio( surf[s].l[i], surf[s].typ[i], 0.5, lnew_, typnew);
    if (ps[1]==-1) { printf(" ERROR: 5-sided body not meshable\n"); return(-1); }

    /* create a 4 sided surf */
    for (jj=0; jj<i; jj++)
    {
      edge[jj]=surf[s].l[jj];
      cori[jj]=surf[s].o[jj];
      ctyp[jj]=surf[s].typ[jj];
    }
    edge[jj]=lnew_[0];
    cori[jj]=surf[s].o[2];
    ctyp[jj]=typnew[0];
    jj++;
    edge[jj]=lnew_[1];
    cori[jj]=surf[s].o[2];
    ctyp[jj]=typnew[1];
    for (; jj<3; jj++) /* thats ok so */
    {
      edge[jj+1]=surf[s].l[jj];
      cori[jj+1]=surf[s].o[jj];
      ctyp[jj+1]=surf[s].typ[jj];
    }

#if TEST
    for (jj=0; jj<MAX_EDGES_PER_SURF; jj++)
    {
      if(ctyp[jj]=='l') printf("edge:%s cori:%c ctyp:%c\n", line[edge[jj]].name, cori[jj], ctyp[jj]);
      else if(ctyp[jj]=='c') printf("edge:%s cori:%c ctyp:%c\n", lcmb[edge[jj]].name, cori[jj], ctyp[jj]);
      else printf("error:%c\n",ctyp[jj]);
    }
#endif
    ss[1]=surface_i( surf[ss[1]].name, surf[s].ori, -1, MAX_EDGES_PER_SURF, cori, edge, ctyp );
    if( ss[1]<0)
    { printf("ERROR: surface could not be created\n"); return(-1); }
    setr( 0, "s",ss[1] );        
    surf[ss[1]].etyp=surf[s].etyp;
    /* update the reference to the substitute-surface (s)  */
    surf[s].l[surf[s].nl]=i ;
    surf[s].l[surf[s].nl+1]=ss[1];
    surf[s].l[surf[s].nl+2]=ps[1];
    surf[s].l[surf[s].nl+3]=ls[1][0]=lnew_[0];
    surf[s].l[surf[s].nl+4]=ts[1][0]=typnew[0];
    surf[s].l[surf[s].nl+5]=ls[1][1]=lnew_[1];
    surf[s].l[surf[s].nl+6]=ts[1][1]=typnew[1];

    if(printFlag) printf("surf[%d]:%s is replaced by surf[%d]:%s\n",j, surf[s].name,s, surf[ss[1]].name);

    /* the embedded nodes must be rearranged to meet the toplology of the new slave surf */
    /* allocate memory for embeded nodes */
    if((surf[ss[1]].nod=(int *)realloc((int *)surf[ss[1]].nod, surf[s].nn*sizeof(int)) )==NULL)
    { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->nod (3)\n\n", surf[ss[1]].name);
      return(-1); }

    /* map the nodes */
    /* WARNING: this straight forward mapping is not supposed to work but it works thanks to the meshimprover. */
    /* the changed u,v range is not considered, the necessary order is differend and should be updated */
    surf[ss[1]].nn=surf[s].nn;
    for(j=0; j<surf[ss[1]].nn; j++) surf[ss[1]].nod[j]=surf[s].nod[j];
  }
#if TEST
  for(bs=0; bs<2; bs++) 
  {
    printf("ps:%s sm:%s ss:%s\n", point[ps[bs]].name, surf[sm[bs]].name, surf[ss[bs]].name);
    if(tm[bs]=='c') printf("cm:%s\n", lcmb[lm[bs]].name );
    if(tm[bs]=='l') printf("lm:%s\n", line[lm[bs]].name );
    if(ts[bs][0]=='c') printf("cs:%s\n", lcmb[ls[bs][0]].name );
    if(ts[bs][0]=='l') printf("ls:%s\n", line[ls[bs][0]].name );
    if(ts[bs][1]=='c') printf("cs:%s\n", lcmb[ls[bs][1]].name );
    if(ts[bs][1]=='l') printf("ls:%s\n", line[ls[bs][1]].name );
  }
#endif

  /* replace this surf sm by two new ones ss[4 and 5] and map the nodes into them */
  /* search the remaining unknown lines between ss 0 and 1 */
  j=0; for(i=0; i<surf[sm[0]].nl; i++)
  {
    /* if the lines are not lm[] */
    if( ((surf[sm[0]].typ[i]==tm[0])&&(surf[sm[0]].l[i]==lm[0])) || ((surf[sm[0]].typ[i]==tm[1])&&(surf[sm[0]].l[i]==lm[1])) );
    else
    {
      tm2[j]=surf[sm[0]].typ[i]; 
      om2[j]=surf[sm[0]].o[i]; 
      lm2[j]=surf[sm[0]].l[i];
#if TEST
      if(tm2[j]=='l') printf("%d line between sur0,1:%s\n", j,line[lm2[j]].name); 
      else if(tm2[j]=='c') printf("%d lcmb between sur0,1:%s\n", j,lcmb[lm2[j]].name); 
      else printf("%d typ:%c not known, lm2:%d\n", j, tm2[j],lm2[j] );
#endif
      j++;
    }
  }
  /* create a line between ps and fill it with nodes from sm */
  getNewName( name, "l" );
  lnew= line_i( name, ps[0], ps[1], -1, line[lm2[0]].div, 1, 0 );
  if(lnew<0) { printf("ERROR: line could not be created\n"); return(-1); }
  pre_seta(specialset->zap, "l", name);

  /* create the surfs ss 4 and 5 */
  /* the 0. line of the slave surfs correspond to lm2 and the orientation also */
  /* look which ls are connected to lm2 */
  for(i=0; i<2; i++) /* two surfs inside sm[0] */ 
  {
#if TEST
    printf(" create new surf\n"); 
#endif
    if((tm2[i]!='l')&&(tm2[i]!='c'))
    {
      printf("ERROR: body:%s could not be meshed\n", body[*b].name);
      return(-1);
    }

    if(om2[i]=='-') { l1=1; l3=3; }    
    else            { l1=3; l3=1; }    
    for(j=0; j<2; j++) /* left and right side of sm */
    {
      for(l=0; l<2; l++) /* two lines on each side */
      {
        if(tm2[i]=='l')
	{
          if(ts[j][l]=='l')
          {
            if((line[lm2[i]].p1==line[ls[j][l]].p1)||(line[lm2[i]].p1==line[ls[j][l]].p2))
            {
              clin[l1]=ls[j][l];
              ctyp[l1]=ts[j][l];
	    }
            if((line[lm2[i]].p2==line[ls[j][l]].p1)||(line[lm2[i]].p2==line[ls[j][l]].p2))
            {
              clin[l3]=ls[j][l];
              ctyp[l3]=ts[j][l];
	    }
	  }
          if(ts[j][l]=='c')
          {
            if((line[lm2[i]].p1==lcmb[ls[j][l]].p1)||( line[lm2[i]].p1==lcmb[ls[j][l]].p2))
            {
              clin[l1]=ls[j][l];
              ctyp[l1]=ts[j][l];
	    }
            if((line[lm2[i]].p2==lcmb[ls[j][l]].p1)||( line[lm2[i]].p2==lcmb[ls[j][l]].p2))
            {
              clin[l3]=ls[j][l];
              ctyp[l3]=ts[j][l];
	    }
	  }
	}
        if(tm2[i]=='c')
	{
          if(ts[j][l]=='l')
          {
            if((lcmb[lm2[i]].p1==line[ls[j][l]].p1)||( lcmb[lm2[i]].p1==line[ls[j][l]].p2))
            {
              clin[l1]=ls[j][l];
              ctyp[l1]=ts[j][l];
            }
            if((lcmb[lm2[i]].p2==line[ls[j][l]].p1)||( lcmb[lm2[i]].p2==line[ls[j][l]].p2))
            {
              clin[l3]=ls[j][l];
              ctyp[l3]=ts[j][l];
	    }
	  }
          if(ts[j][l]=='c')
          {
            if((lcmb[lm2[i]].p1==lcmb[ls[j][l]].p1)||( lcmb[lm2[i]].p1==lcmb[ls[j][l]].p2))
            {
              clin[l1]=ls[j][l];
              ctyp[l1]=ts[j][l];
            }
            if((lcmb[lm2[i]].p2==lcmb[ls[j][l]].p1)||( lcmb[lm2[i]].p2==lcmb[ls[j][l]].p2))
            {
              clin[l3]=ls[j][l];
              ctyp[l3]=ts[j][l];
	    }
	  }
	}
      }
    }

    clin[0]=lm2[i];
    ctyp[0]=tm2[i];
    clin[2]=lnew;
    ctyp[2]='l';
    for(j=0; j<6; j++) cori[j]='+';
#if TEST
    for(j=0; j<4; j++)
    {
      if(ctyp[j]=='l')printf("clin[%d]:%s  ctyp:%c cori:%c\n", j, line[clin[j]].name,ctyp[j],cori[j]);
      else if(ctyp[j]=='c')printf("clin[%d]:%s  ctyp:%c cori:%c\n", j, lcmb[clin[j]].name,ctyp[j],cori[j]);
      else printf("typ:%c not known, clin:%d cori:%c\n", ctyp[j],clin[j],cori[j] );
    }
#endif
    getNewName( name, "s" );
    ss[i+4]= surface_i( name, surf[sm[0]].ori, -1, 4, cori, clin, ctyp );
    if(ss[i+4]<0)
    {
      printf("ERROR: surf could not be created\n");
      return(-1);
    }
    //setr( 0, "s", ss[i+4] );
    pre_seta(specialset->zap, "s", name);

    /* map the nodes of the master to the slave */
    /* determine the div of the surf umax and vmax */
    k=surf[sm[0]].l[0];
    umax=0;
    if( surf[sm[0]].typ[0]=='l' ) umax=line[k].div;
    else  for( l=0; l<lcmb[k].nl; l++ ) umax+=line[lcmb[k].l[l]].div;
    k=surf[sm[0]].l[3];
    vmax=0;
    if( surf[sm[0]].typ[3]=='l' ) vmax=line[k].div;
    else  for( l=0; l<lcmb[k].nl; l++ ) vmax+=line[lcmb[k].l[l]].div;

    if((sn = (int **)malloc((umax+1)*sizeof(int *))) == NULL )
      errMsg(" ERROR: malloc failed in bodyFrom5Surfs()\n"); 
    for(k=0; k<(umax+1); k++)
    {
      if((sn[k] = (int *)malloc((vmax+1)*sizeof(int))) == NULL )
        errMsg(" ERROR: malloc failed in bodyFrom5Surfs()\n");
    }

    /* map the master-surf-nodes in a prelim-area */

    /* define the division of surf[].l[1] which is aligned to lm[]. */
    /* this is necessary to determine the ratio of nodes of both slave surfs */
    if(surf[ss[i+4]].typ[1]=='l') ii=line[surf[ss[i+4]].l[1]].div;
    else
    {
      ii=0;
      for(u=0; u<lcmb[surf[ss[i+4]].l[1]].nl; u++)
      {
        ii+=line[lcmb[surf[ss[i+4]].l[1]].l[u]].div;
      }
    }
    j=0; for (u=1; u<umax; u++)
    {
      for (v=1; v<vmax; v++)
      {
        sn[u][v]=surf[sm[0]].nod[j];
        j++;
      }
    }

    j=0;
    if((lm2[i]==surf[sm[0]].l[0])&&(tm2[i]==surf[sm[0]].typ[0]))
    { 
      if((surf[ss[i+4]].nod=(int *)realloc((int *)surf[ss[i+4]].nod, (umax*vmax/2+1)*sizeof(int)) )==NULL)
        errMsg(" ERROR: malloc failure\n");
      for (u=1; u<umax; u++)
      {
        for (v=1; v<ii; v++)
        {
          surf[ss[i+4]].nod[j]=sn[u][v];
	  /* printf("ns00[%d][%d]:%d\n", u,v,sn[u][v]); */
          j++;
	}
      }
      /* add nodes to the new line */
      if ((line[lnew].nod = (int *)realloc( (int *)line[lnew].nod, (umax+1)*sizeof(int)) ) == NULL )
        errMsg(" ERROR: malloc failure\n");
      n=0; if(surf[ss[i+4]].o[2]=='+')
      {
        for (u=umax-1; u>0; u--) { line[lnew].nod[n]= sn[u][ii]; n++; }
      }
      else
      {
        for (u=1; u<umax; u++) { line[lnew].nod[n]= sn[u][ii]; n++; }
      }
      line[lnew].nn=n;
    }
    if((lm2[i]==surf[sm[0]].l[1])&&(tm2[i]==surf[sm[0]].typ[1]))
    { 
      if((surf[ss[i+4]].nod=(int *)realloc((int *)surf[ss[i+4]].nod, (umax/2*vmax+1)*sizeof(int)) )==NULL)
        errMsg(" ERROR: malloc failure\n");
      for (v=1; v<vmax; v++)
      {
        for (u=umax-1; u>(umax-ii); u--)
        {
          surf[ss[i+4]].nod[j]=sn[u][v];
          j++;
	}
      }
      /* add nodes to the new line */
      if ((line[lnew].nod = (int *)realloc( (int *)line[lnew].nod, (vmax+1)*sizeof(int)) ) == NULL )
        errMsg(" ERROR: malloc failure\n");
      n=0; if(surf[ss[i+4]].o[2]=='+')
      {
        for (v=vmax-1; v>0; v--) { line[lnew].nod[n]= sn[umax-ii][v]; n++; }
      }
      else
      {
        for (v=1; v<vmax; v++) { line[lnew].nod[n]= sn[umax-ii][v]; n++; }
      }
      line[lnew].nn=n;
    }
    if((lm2[i]==surf[sm[0]].l[2])&&(tm2[i]==surf[sm[0]].typ[2]))
    { 
      if((surf[ss[i+4]].nod=(int *)realloc((int *)surf[ss[i+4]].nod, (umax*vmax/2+1)*sizeof(int)) )==NULL)
        errMsg(" ERROR: malloc failure\n");
      for (u=umax-1; u>0; u--)
      {
        for (v=vmax-1; v>(vmax-ii); v--)
        {
          surf[ss[i+4]].nod[j]=sn[u][v];
          j++;
	}
      }
    }
    if((lm2[i]==surf[sm[0]].l[3])&&(tm2[i]==surf[sm[0]].typ[3]))
    { 
      if((surf[ss[i+4]].nod=(int *)realloc((int *)surf[ss[i+4]].nod, (umax/2*vmax+1)*sizeof(int)) )==NULL)
        errMsg(" ERROR: malloc failure\n");
      for (v=vmax-1; v>0; v--)
      {
        for (u=1; u<ii; u++)
        {
          surf[ss[i+4]].nod[j]=sn[u][v];
          j++;
	}
      }
    }
    surf[ss[i+4]].nn=j;

    for(k=0; k<(umax+1); k++) free(sn[k]);
    free(sn);
  }

  /* create a new body with this surfs */ 
  j=2; for(i=2; i<5; i++)
  {
    if(body[*b].s[i]==sm[0]) { ss[j]=ss[4]; j++; ss[j]=ss[5]; }
    else ss[j]=body[*b].s[i];
    j++;
  }
#if TEST
  for(i=0; i<6; i++)
  {
    printf("body: s:%s c:%c\n", surf[ss[i]].name, cori[i]);
  }
#endif
  getNewName( name, "b" );
  bs= gbod_i( name, -1, 6, cori, ss );
  if(bs<0) { printf("ERROR: body could not be created\n"); return(-1); }
  setr(0,"b",bs);
  pre_seta(specialset->zap, "b", name);
  body[bs].etyp=body[*b].etyp;
  body[bs].eattr=body[*b].eattr;
  *b=bs;
  return(1);
}


int bodyFrom7Surfs( int *b)
{
  int i,j,jj,k,l,n,s, **sn=NULL, u,v, umax0,umax1,vmax0,vmax1, ps=-1;
  int bs;          /* body-surf */
  int lm[2][2];       /* master-lines which are replaced by lines ls */
  int tm[2][2];       /* master-line-type which is replaced by lines ls */
  int ss[6];       /* slave surfs */
  int cs[2];    /* lcmb cs[surf] in the place of the master-lines */
  int sm[2][2];       /* surfaces between 0 and 1 which are to be replaced by a new one */

  int sml[2][4];     /* either 0 or 1 if the line is identified by lm or lcomon, lm2 are the remainders which are not connected with lcommon */
  int lm2[2]={0,0};        /* line-index of the lines between surf 0 and 1 which are needed for ss */
  int tm2[2]={0,0};        /* line-type of the lines between surf 0 and 1 which are needed for ss */
  int cl[2];

  int tcomon=0, ocomon, lcomon=0; /* line between sm */ 
  int clin[5];
  int edge[5];
  char ctyp[5]; 
  char cori[MAX_SURFS_PER_BODY];  /* orientation, used for slave surfs and body */

  char name[MAX_LINE_LENGTH];


  /* store the basic info from the substitute surfs */
  for(bs=0; bs<2; bs++)
  {
    s=body[*b].s[bs];
    lm[bs][0]=surf[s].l[surf[s].l[5]];
    tm[bs][0]=surf[s].typ[surf[s].l[5]];
    lm[bs][1]=surf[s].l[surf[s].l[6]];
    tm[bs][1]=surf[s].typ[surf[s].l[6]];
    ss[bs]=surf[s].l[7];
    cs[bs]=surf[s].l[8];
#if TEST
    printf("s:%d = %s\n", s, surf[s].name);
    printf("tm0:%c tm1:%c l5:%d l6:%d\n",tm[bs][0], tm[bs][1], surf[s].l[5],surf[s].l[6] );
    if(tm[bs][0]=='c') printf(" cm0:%s\n", lcmb[lm[bs][0]].name );
    if(tm[bs][0]=='l') printf(" lm0:%s\n", line[lm[bs][0]].name );
    if(tm[bs][1]=='c') printf(" cm1:%s\n", lcmb[lm[bs][1]].name );
    if(tm[bs][1]=='l') printf(" lm1:%s\n", line[lm[bs][1]].name );
    printf(" ss[%d]:%s\n", ss[bs], surf[ss[bs]].name);
    printf(" cs[%d]:%s\n", cs[bs], lcmb[cs[bs]].name );
#endif
  }

  /* search the two surfs sm between the 5-sided surfs which are related to the lm[0][0 and 1] */
  for(bs=0; bs<2; bs++) 
  {
    n=0;
    sm1_found:;
    for(i=2; i<7; i++) /* all possible surfs of the body */
    {
      sm[bs][n]=body[*b].s[i];
      for(k=0; k<surf[sm[bs][n]].nl; k++) /* compare with all lines/lcmb in this surf */
      {
#if TEST
        printf("bs:%d k:%d n:%d check line:%s i:%d with i:%d of typ:%c\n", bs, k, n, line[surf[sm[bs][n]].l[k]].name, surf[sm[bs][n]].l[k], lm[bs][n], tm[bs][n]);
#endif
        if((surf[sm[bs][n]].l[k]==lm[bs][n])&&(surf[sm[bs][n]].typ[k]==tm[bs][n]))
        {
          sml[bs][k]=1; /* mark the line as known */
#if TEST
          if(surf[sm[bs][n]].typ[k]=='l') printf("found surf:%s at line:%s\n", surf[sm[bs][n]].name, line[surf[sm[bs][n]].l[k]].name);
          if(surf[sm[bs][n]].typ[k]=='c') printf("found surf:%s at lcmb:%s\n", surf[sm[bs][n]].name, lcmb[surf[sm[bs][n]].l[k]].name);
#endif
          n++;
          if(n==1) goto sm1_found;
          else goto sm2_found;
        }
      }  
    }
    sm2_found:;
  }

  /* compare the connected surfs */
  jj=0; for(i=0; i<2; i++) for(j=0; j<2; j++) if(sm[0][i]==sm[1][j]) jj++;

  if(jj!=2)
  {
    /* the embedded surface ss[1] must be redefined. */

    /* this works only if the number of embedded nodes of body[*b].s[0] and body[*b].s[1] are equal   */
    /* to overcome this problem a remeshing of one of this surfs would be necessary, but then already */
    /* created elements based on this surface would not fit */
    if(surf[body[*b].s[0]].nn!=surf[body[*b].s[1]].nn) 
    { printf(" ERROR: 7-sided body not meshable, the distortion is probably to big\n"); return(-1); }

    /* search the corresponding line to lm[0] on surf sm[0][0]. This is also part of surf =body[*b].s[1] */
    cl[0]=cl[1]=-1;
    s=body[*b].s[1];
    for(i=0; i<5; i++)
      for(j=0; j<4; j++)
        if((surf[sm[0][0]].l[j]==surf[s].l[i])&&(surf[sm[0][0]].typ[j]==surf[s].typ[i]))
        { cl[0]=i; goto foundLine0; }
   foundLine0:;
    if(cl[0]==-1) { printf(" ERROR: 7-sided body not meshable 0\n"); return(-1); }
    for(i=0; i<5; i++)
      for(j=0; j<4; j++)
        if((surf[sm[0][1]].l[j]==surf[s].l[i])&&(surf[sm[0][1]].typ[j]==surf[s].typ[i]))
        { cl[1]=i; goto foundLine1; }
   foundLine1:;
    if(cl[1]==-1) { printf(" ERROR: 7-sided body not meshable 1\n"); return(-1); }

    /* create a lcmb out of 2 edges */
    cs[1]=addTwoLines( surf[s].l[cl[0]], surf[s].o[cl[0]], surf[s].typ[cl[0]], surf[s].l[cl[1]], surf[s].l[cl[1]], surf[s].typ[cl[1]] );
    if( cs[1]==-1)  { printf(" ERROR: 7-sided body not meshable 2\n"); return(-1); };

    /* create a 4 sided surf */
    if (cl[0]==4)
    {
      edge[0]=cs[1];
      cori[0]=surf[s].o[cl[0]];
      ctyp[0]='c';
      for (jj=1; jj<cl[0]; jj++)
      {
        edge[jj]=surf[s].l[jj];
        cori[jj]=surf[s].o[jj];
        ctyp[jj]=surf[s].typ[jj];
      }
    }
    else
    {
      for (jj=0; jj<cl[0]; jj++)
      {
        edge[jj]=surf[s].l[jj];
        cori[jj]=surf[s].o[jj];
        ctyp[jj]=surf[s].typ[jj];
      }
      edge[jj]=cs[1];
      cori[jj]=surf[s].o[cl[0]];
      ctyp[jj]='c';
      for (jj++; jj<4; jj++)
      {
        edge[jj]=surf[s].l[jj+1];
        cori[jj]=surf[s].o[jj+1];
        ctyp[jj]=surf[s].typ[jj+1];
      }
    }
#if TEST
    for (jj=0; jj<MAX_EDGES_PER_SURF; jj++)
    {
      if((ctyp[jj]!='l')&&(ctyp[jj]!='c'))
      { printf("ERROR: body:%s could not be meshed\n", body[*b].name); return(-1); }
      if(ctyp[jj]=='l') printf("edge:%s cori:%c ctyp:%c\n", line[edge[jj]].name, cori[jj], ctyp[jj]);
      else if(ctyp[jj]=='c') printf("edge:%s cori:%c ctyp:%c\n", lcmb[edge[jj]].name, cori[jj], ctyp[jj]);
      else printf("error:%c\n",ctyp[jj]);
    }
#endif
    ss[1]=surface_i( surf[ss[1]].name, surf[ss[1]].ori, -1, MAX_EDGES_PER_SURF, cori, edge, ctyp );
    if(ss[1]<0) { printf("ERROR: surf could not be created\n"); return(-1); }
    setr( 0, "s", ss[1] );
    surf[ss[1]].etyp=surf[s].etyp;
    
    surf[s].l[5]=cl[0];    /* place of the line in the surf */
    surf[s].l[6]=cl[1];  /* place of the line in the surf */
    surf[s].l[7]=ss[1];  /* slave surf */
    surf[s].l[8]=cs[1];  /* slave lcmb */

    lm[1][0]=surf[s].l[surf[s].l[5]];
    tm[1][0]=surf[s].typ[surf[s].l[5]];
    lm[1][1]=surf[s].l[surf[s].l[6]];
    tm[1][1]=surf[s].typ[surf[s].l[6]];

    if(printFlag) printf("surf:%s is replaced by surf:%s\n", surf[s].name, surf[ss[1]].name);

    /* the embedded nodes must be rearranged to meet the toplology of the new slave surf */
    /* allocate memory for embeded nodes */
    if((surf[ss[1]].nod=(int *)realloc((int *)surf[ss[1]].nod, surf[s].nn*sizeof(int)) )==NULL)
    { printf(" ERROR: realloc failure in meshSurfs surf:%s can not be meshed, surf->nod (4)\n\n", surf[ss[1]].name);
      return(-1); }

    /* map the nodes */
    /* WARNING: this straight forward mapping is not supposed to work but it works thanks to the mesh-improver. */
    /* the changed u,v range is not considered, the necessary order is differend and should be updated */
    surf[ss[1]].nn=surf[s].nn;
    for(j=0; j<surf[ss[1]].nn; j++) surf[ss[1]].nod[j]=surf[s].nod[j];
  }

  /* search the line between sm 0 and 1 */
  /* reset sml */
  for(i=0; i<4; i++) sml[0][i]=sml[1][i]=0;
  for(i=0; i<surf[sm[0][0]].nl; i++)
  {
    for(j=0; j<surf[sm[0][1]].nl; j++)
    {
      if(surf[sm[0][0]].l[i]==surf[sm[0][1]].l[j])
      {
        sml[0][i]=1; /* mark the line as known */
        tcomon=surf[sm[0][0]].typ[i]; 
        ocomon=surf[sm[0][0]].o[i]; 
        lcomon=surf[sm[0][0]].l[i];
#if TEST
  if(tcomon=='l') printf(" line between sm 0:%s ,1:%s =%s\n", surf[sm[0][0]].name, surf[sm[0][1]].name,line[lcomon].name); 
  if(tcomon=='c') printf(" lcmb between sm 0:%s ,1:%s =%s\n", surf[sm[0][0]].name, surf[sm[0][1]].name,lcmb[lcomon].name); 
#endif
      }
    }
  }

  /* find the outer lines lm2 */

  /* mark all surf-lines which are identical to lm */
  for(i=0; i<4; i++) for(j=0; j<2; j++) for(k=0; k<2; k++) for(n=0; n<2; n++)
    if((surf[sm[0][j]].l[i]==lm[k][n])&&(surf[sm[0][j]].typ[i]==tm[k][n])) sml[j][i]=1;

  /* mark all surf-lines which are identical to lcomon */
  for(i=0; i<4; i++) for(j=0; j<2; j++) 
    if((surf[sm[0][j]].l[i]==lcomon)&&(surf[sm[0][j]].typ[i]==tcomon)) sml[j][i]=1;

  /* look which lines are unmarked on each 1st surf relating to the 5sided surfs */
  for(i=0; i<4; i++) if(sml[0][i]==0) { lm2[0]=surf[sm[0][0]].l[i]; tm2[0]=surf[sm[0][0]].typ[i]; }
  for(i=0; i<4; i++) if(sml[1][i]==0) { lm2[1]=surf[sm[0][1]].l[i]; tm2[1]=surf[sm[0][1]].typ[i]; }

#if TEST
 if(tm2[0]=='c') printf(" lm20:%s \n", lcmb[lm2[0]].name);
 else            printf(" lm20:%s \n", line[lm2[0]].name);
 if(tm2[1]=='c') printf(" lm21:%s \n", lcmb[lm2[1]].name);
 else            printf(" lm21:%s \n", line[lm2[1]].name);
#endif

  /* replace this surfs sm by a new one ss and map the nodes into it */
  clin[0]=cs[0];
  ctyp[0]='c';
  clin[1]=lm2[0];
  ctyp[1]=tm2[0];
  clin[2]=cs[1];
  ctyp[2]='c';
  clin[3]=lm2[1];
  ctyp[3]=tm2[1];
  for(j=0; j<6; j++) cori[j]='+';
#if TEST
  for(j=0; j<4; j++)
  {
      if(ctyp[j]=='l')printf("clin[%d]:%s  ctyp:%c cori:%c\n", j, line[clin[j]].name,ctyp[j],cori[j]);
      else if(ctyp[j]=='c')printf("clin[%d]:%s  ctyp:%c cori:%c\n", j, lcmb[clin[j]].name,ctyp[j],cori[j]);
      else printf("typ:%c not known\n", ctyp[j]);
  }
#endif
  getNewName( name, "s" );
  ss[5]= surface_i( name, surf[sm[0][0]].ori, -1, 4, cori, clin, ctyp );
  if(ss[5]<0) { printf("ERROR: surf could not be created\n"); return(-1); }
  setr( 0, "s", ss[5] );
  pre_seta(specialset->zap, "s", name);

  /* map the nodes of the master-surfs into the slave-surf */
  /* determine the div of the surf */
  k=surf[sm[0][0]].l[0];
  umax0=0;
  if( surf[sm[0][0]].typ[0]=='l' ) umax0=line[k].div;
  else  for( l=0; l<lcmb[k].nl; l++ ) umax0+=line[lcmb[k].l[l]].div;
  k=surf[sm[0][1]].l[0];
  umax1=0;
  if( surf[sm[0][1]].typ[0]=='l' ) umax1=line[k].div;
  else  for( l=0; l<lcmb[k].nl; l++ ) umax1+=line[lcmb[k].l[l]].div;

  k=surf[sm[0][0]].l[3];
  vmax0=0;
  if( surf[sm[0][0]].typ[3]=='l' ) vmax0=line[k].div;
  else  for( l=0; l<lcmb[k].nl; l++ ) vmax0+=line[lcmb[k].l[l]].div;
  k=surf[sm[0][1]].l[3];
  vmax1=0;
  if( surf[sm[0][1]].typ[3]=='l' ) vmax1=line[k].div;
  else  for( l=0; l<lcmb[k].nl; l++ ) vmax1+=line[lcmb[k].l[l]].div;

  /* allocate space for the nodes of the slave-surf, for convenience it is allocated too big */
  if((surf[ss[5]].nod=(int *)realloc((int *)surf[ss[5]].nod,((umax0+umax1)*(vmax0+vmax1)+1)*sizeof(int)))==NULL)
    errMsg(" ERROR: realloc failure\n");

  /* running-number of slave-surf-nodes */
  n=0; 

  /* store the nodes of the master sm[0][1] into nod[u][v] */
  if((sn = (int **)malloc((umax1+1)*sizeof(int *))) == NULL )
    errMsg(" ERROR: malloc failed in bodyFrom7Surfs()\n"); 
  for(k=0; k<(umax1+1); k++)
  {
    if((sn[k] = (int *)malloc((vmax1+1)*sizeof(int))) == NULL )
      errMsg(" ERROR: malloc failed in bodyFrom7Surfs()\n");
  }
  j=0; for (u=1; u<umax1; u++)
  {
    for (v=1; v<vmax1; v++)
    {
      sn[u][v]=surf[sm[0][1]].nod[j];
      j++;
    }
  }
#if TEST
  printf("umax0:%d vmax0:%d umax1:%d vmax1:%d\n", umax0, vmax0, umax1, vmax1);
  printf("sm[0][1]:%s v:lm2[1]:%d u:lm[0][1]:%d\n", surf[sm[0][1]].name, lm2[1], lm[0][1] );
  for (v=0; v<4; v++) printf("surf_l[%d]:%d\n",v,surf[sm[0][1]].l[v]);
#endif
  if((surf[sm[0][1]].l[0]==lm2[1])&&(surf[sm[0][1]].l[3]==lm[0][1]))
  { 
    for (v=1; v<vmax1; v++)
    {
      for (u=1; u<umax1; u++)
      {
        surf[ss[5]].nod[n]=sn[u][v];
        n++;
      }
    }
  }
  else if((surf[sm[0][1]].l[2]==lm2[1])&&(surf[sm[0][1]].l[1]==lm[0][1]))
  { 
    for (v=vmax1-1; v>0; v--)
    {
      for (u=umax1-1; u>0; u--)
      {
        surf[ss[5]].nod[n]=sn[u][v];
        n++;
      }
    }
  }
  else if((surf[sm[0][1]].l[3]==lm2[1])&&(surf[sm[0][1]].l[2]==lm[0][1]))
  { 
    for (u=1; u<umax1; u++)
    {
      for (v=vmax1-1; v>0; v--)
      {
        surf[ss[5]].nod[n]=sn[u][v];
        n++;
      }
    }
  }
  else if((surf[sm[0][1]].l[1]==lm2[1])&&(surf[sm[0][1]].l[0]==lm[0][1]))
  { 
    for (u=umax1-1; u>0; u--)
    {
      for (v=1; v<vmax1; v++)
      {
        surf[ss[5]].nod[n]=sn[u][v];
        n++;
      }
    }
  }
  else if((surf[sm[0][1]].l[0]==lm2[1])&&(surf[sm[0][1]].l[1]==lm[0][1]))
  {
    for (v=1; v<vmax1; v++)
    {
      for (u=umax1-1; u>0; u--)
      {
        surf[ss[5]].nod[n]=sn[u][v];
        n++;
      }
    }
  }
  else if((surf[sm[0][1]].l[2]==lm2[1])&&(surf[sm[0][1]].l[3]==lm[0][1]))
  { 
    for (v=vmax1-1; v>0; v--)
    {
      for (u=1; u<umax1; u++)
      {
        surf[ss[5]].nod[n]=sn[u][v];
        n++;
      }
    }
  }
  else if((surf[sm[0][1]].l[3]==lm2[1])&&(surf[sm[0][1]].l[0]==lm[0][1]))
  { 
    for (u=1; u<umax1; u++)
    {
      for (v=1; v<vmax1; v++)
      {
        surf[ss[5]].nod[n]=sn[u][v];
        n++;
      }
    }
  }
  else if((surf[sm[0][1]].l[1]==lm2[1])&&(surf[sm[0][1]].l[2]==lm[0][1]))
  { 
    for (u=umax1-1; u>0; u--)
    {
      for (v=vmax1-1; v>0; v--)
      {
        surf[ss[5]].nod[n]=sn[u][v];
        n++;
      }
    }
  }
  else
  {
    errMsg("BUG: found no surface orientation for surf:%s\n", surf[sm[0][1]].name );
    exit(-1);
  }
  for(k=0; k<(umax1+1); k++) free(sn[k]);
  free(sn);

  /* add the nodes of lcomon */
  /* search the index of the point between lm[0][0] and lm[0][1] */
  if(( tm[0][0]=='l')&&( tm[0][1]=='l'))
  {
    if( line[lm[0][0]].p1==line[lm[0][1]].p1) ps=line[lm[0][0]].p1;
    else if( line[lm[0][0]].p1==line[lm[0][1]].p2) ps=line[lm[0][0]].p1;
    else if( line[lm[0][0]].p2==line[lm[0][1]].p2) ps=line[lm[0][0]].p2;
    else if( line[lm[0][0]].p2==line[lm[0][1]].p1) ps=line[lm[0][0]].p2;
  }  
  else if(( tm[0][0]=='l')&&( tm[0][1]=='c'))
  {
    if( line[lm[0][0]].p1==lcmb[lm[0][1]].p1) ps=line[lm[0][0]].p1;
    else if( line[lm[0][0]].p1==lcmb[lm[0][1]].p2) ps=line[lm[0][0]].p1;
    else if( line[lm[0][0]].p2==lcmb[lm[0][1]].p2) ps=line[lm[0][0]].p2;
    else if( line[lm[0][0]].p2==lcmb[lm[0][1]].p1) ps=line[lm[0][0]].p2;
  }  
  else if(( tm[0][0]=='c')&&( tm[0][1]=='l'))
  {
    if( lcmb[lm[0][0]].p1==line[lm[0][1]].p1) ps=lcmb[lm[0][0]].p1;
    else if( lcmb[lm[0][0]].p1==line[lm[0][1]].p2) ps=lcmb[lm[0][0]].p1;
    else if( lcmb[lm[0][0]].p2==line[lm[0][1]].p2) ps=lcmb[lm[0][0]].p2;
    else if( lcmb[lm[0][0]].p2==line[lm[0][1]].p1) ps=lcmb[lm[0][0]].p2;
  }  
  else if(( tm[0][0]=='c')&&( tm[0][1]=='c'))
  {
    if( lcmb[lm[0][0]].p1==lcmb[lm[0][1]].p1) ps=lcmb[lm[0][0]].p1;
    else if( lcmb[lm[0][0]].p1==lcmb[lm[0][1]].p2) ps=lcmb[lm[0][0]].p1;
    else if( lcmb[lm[0][0]].p2==lcmb[lm[0][1]].p2) ps=lcmb[lm[0][0]].p2;
    else if( lcmb[lm[0][0]].p2==lcmb[lm[0][1]].p1) ps=lcmb[lm[0][0]].p2;
  }  
  if(ps<0) { printf("ERROR: ps:%d can not be determined\n", ps); exit(-1); }
    
  /* look how the line is oriented relative to ps */
  if(tcomon=='l')
  {
    if(ps==line[lcomon].p1)
    {
      for (v=0; v<line[lcomon].div-1; v++)
      {
        surf[ss[5]].nod[n]=line[lcomon].nod[v];
        n++;
      }
    }
    else
    {
      for (v=line[lcomon].div-2; v>-1; v--)
      {
        surf[ss[5]].nod[n]=line[lcomon].nod[v];
        n++;
      }
    }
  }
  else /* tcomon=='c' */
  {
    if(ps==lcmb[lcomon].p1)
    {
      for( l=0; l<lcmb[lcomon].nl; l++ )
      {
        if(lcmb[lcomon].o[l]=='+')
        {
          for (v=0; v<line[lcmb[lcomon].l[l]].div-1; v++)
          {
            surf[ss[5]].nod[n]=line[lcmb[lcomon].l[l]].nod[v];
            n++;
          }
          /* add the node at the end of the line */
          surf[ss[5]].nod[n]=point[line[lcmb[lcomon].l[l]].p2].nod[0];
          n++;
        }
        else
        {
          for (v=line[lcmb[lcomon].l[l]].div-2; v>-1; v--)
          {
            surf[ss[5]].nod[n]=line[lcmb[lcomon].l[l]].nod[v];
            n++;
          }
          /* add the node at the end of the line */
          surf[ss[5]].nod[n]=point[line[lcmb[lcomon].l[l]].p1].nod[0];
          n++;
        }
      }
    }
    else
    {
      for( l=0; l<lcmb[lcomon].nl; l++ )
      {
        if(lcmb[lcomon].o[l]=='-')
        {
          for (v=0; v<line[lcmb[lcomon].l[l]].div-1; v++)
          {
            surf[ss[5]].nod[n]=line[lcmb[lcomon].l[l]].nod[v];
            n++;
          }
          /* add the node at the end of the line */
          surf[ss[5]].nod[n]=point[line[lcmb[lcomon].l[l]].p1].nod[0];
          n++;
        }
        else
        {
          for (v=line[lcmb[lcomon].l[l]].div-2; v>-1; v--)
          {
            surf[ss[5]].nod[n]=line[lcmb[lcomon].l[l]].nod[v];
            n++;
          }
          /* add the node at the end of the line */
          surf[ss[5]].nod[n]=point[line[lcmb[lcomon].l[l]].p2].nod[0];
          n++;
        }
      }
    }
    n--;
  }


  /* store the nodes of the master sm[0][0] into nod[u][v] */
  if((sn = (int **)malloc((umax0+1)*sizeof(int *))) == NULL )
    errMsg(" ERROR: malloc failed in bodyFrom7Surfs()\n"); 
  for(k=0; k<(umax0+1); k++)
  {
    if((sn[k] = (int *)malloc((vmax0+1)*sizeof(int))) == NULL )
      errMsg(" ERROR: malloc failed in bodyFrom7Surfs()\n");
  }
  j=0; for (u=1; u<umax0; u++)
  {
    for (v=1; v<vmax0; v++)
    {
      sn[u][v]=surf[sm[0][0]].nod[j];
      j++;
    }
  }
  if((surf[sm[0][0]].l[0]==lm2[0])&&(surf[sm[0][0]].l[3]==lm[0][0]))
  { 
    for (v=vmax0-1; v>0; v--)
    {
      for (u=1; u<umax0; u++)
      {
        surf[ss[5]].nod[n]=sn[u][v];
        n++;
      }
    }
  }
  else if((surf[sm[0][0]].l[2]==lm2[0])&&(surf[sm[0][0]].l[1]==lm[0][0]))
  { 
    for (v=1; v<vmax0; v++)
    {
      for (u=umax0-1; u>0; u--)
      {
        surf[ss[5]].nod[n]=sn[u][v];
        n++;
      }
    }
  }
  else if((surf[sm[0][0]].l[3]==lm2[0])&&(surf[sm[0][0]].l[2]==lm[0][0]))
  { 
    for (u=umax0-1; u>0; u--)
    {
      for (v=vmax0-1; v>0; v--)
      {
        surf[ss[5]].nod[n]=sn[u][v];
        n++;
      }
    }
  }
  else if((surf[sm[0][0]].l[1]==lm2[0])&&(surf[sm[0][0]].l[0]==lm[0][0]))
  { 
    for (u=1; u<umax0; u++)
    {
      for (v=1; v<vmax0; v++)
      {
        surf[ss[5]].nod[n]=sn[u][v];
        n++;
      }
    }
  }
  else if((surf[sm[0][0]].l[0]==lm2[0])&&(surf[sm[0][0]].l[1]==lm[0][0]))
  { 
    for (v=vmax0-1; v>0; v--)
    {
      for (u=umax0-1; u>0; u--)
      {
        surf[ss[5]].nod[n]=sn[u][v];
        n++;
      }
    }
  }
  else if((surf[sm[0][0]].l[2]==lm2[0])&&(surf[sm[0][0]].l[3]==lm[0][0]))
  { 
    for (v=1; v<vmax0; v++)
    {
      for (u=1; u<umax0; u++)
      {
        surf[ss[5]].nod[n]=sn[u][v];
        n++;
      }
    }
  }
  else if((surf[sm[0][0]].l[3]==lm2[0])&&(surf[sm[0][0]].l[0]==lm[0][0]))
  { 
    for (u=umax0-1; u>0; u--)
    {
      for (v=1; v<vmax0; v++)
      {
        surf[ss[5]].nod[n]=sn[u][v];
        n++;
      }
    }
  }
  else if((surf[sm[0][0]].l[1]==lm2[0])&&(surf[sm[0][0]].l[2]==lm[0][0]))
  { 
    for (u=1; u<umax0; u++)
    {
      for (v=vmax0-1; v>0; v--)
      {
        surf[ss[5]].nod[n]=sn[u][v];
        n++;
      }
    }
  }
  else
  {
    errMsg("BUG: found no surface orientation\n");
    exit(-1);
  }
  for(k=0; k<(umax0+1); k++) free(sn[k]);
  free(sn);

  /* create a new body with this surfs */ 
  j=2; for(i=2; i<7; i++)
  {
    if((body[*b].s[i]!=sm[0][0])&&(body[*b].s[i]!=sm[0][1])) { ss[j]=body[*b].s[i]; j++; }
  }
#if TEST
  for(i=0; i<6; i++)
  {
    printf("body: s:%s c:%c\n", surf[ss[i]].name, cori[i]);
  }
#endif
  getNewName( name, "b" );
  bs= gbod_i( name, -1, 6, cori, ss );
  if(bs<0) { printf("ERROR: body could not be created\n"); return(-1); }
  setr(0,"b",bs);
  pre_seta(specialset->zap, "b", name);
  body[bs].etyp=body[*b].etyp;
  body[bs].eattr=body[*b].eattr;
  *b=bs;
  return(1);
}


/* suche die Eckpunkte der surfaces, setzt einen orientierten body voraus. */
/* beruecksichtige dabei die orientierung der linien oder lcmbs */
/* so das die punktefolge der linienfolge entspricht */
/* return -1 if failed, 1 if successfull */
int generateBodyCornerPoints( int b_indx, int *bcp, int *srefp)
{
  int i, j=0, s, sl, cl, err=0;
  int **scp; /* corner points of the surfs */

  if( (scp=(int **)malloc((body[b_indx].ns+1)*sizeof(int *) ) )==NULL)
  { printf(" ERROR: malloc failure in generateBodyCornerPoints()\n"); return(-1); }
  for(i=0; i<body[b_indx].ns; i++)
  {
    if( (scp[i]=(int *)malloc((surf[body[b_indx].s[i]].nl+1)*sizeof(int) ) )==NULL)
    { printf(" ERROR: malloc failure in generateBodyCornerPoints()\n"); return(-1); }
  }

  for (i=0; i<body[b_indx].ns; i++)
  {
    s=body[b_indx].s[i];
    /* suche den ersten punkt der ersten linie oder lcmb der i-ten surf */
    sl=surf[s].l[0];
    if (surf[s].typ[0]=='c')
    {
      cl=lcmb[sl].nl-1;
      if (surf[s].o[0]=='-')
      {
        if(lcmb[sl].o[cl]=='+') scp[i][0]=line[lcmb[sl].l[cl]].p2;
        else                    scp[i][0]=line[lcmb[sl].l[cl]].p1;
      }
      else
      {
        if(lcmb[sl].o[0]=='+')  scp[i][0]=line[lcmb[sl].l[0]].p1;
        else                    scp[i][0]=line[lcmb[sl].l[0]].p2;
      }
    }
    else if (surf[s].typ[0]=='l')
    {
      if (surf[s].o[0]=='-')
      {
        scp[i][0]=line[surf[s].l[0]].p2;
      }
      else
      {
        scp[i][0]=line[surf[s].l[0]].p1;
      }
    }
    else { errMsg (" ERROR: in meshBodies, surf.typ:%1c not known\n", surf[s].typ[j]); exit(-1);}
  
    /* suche den anfangspunkt aller weiteren linien oder lcmbs der surf */
    for (j=1; j<surf[s].nl; j++)
    {
      sl=surf[s].l[j];
      if (surf[s].typ[j]=='c')
      {
        cl=lcmb[sl].nl-1;
        if (surf[s].o[j]=='-')
        {
          if(lcmb[sl].o[cl]=='+') scp[i][j]=line[lcmb[sl].l[cl]].p2;
          else                    scp[i][j]=line[lcmb[sl].l[cl]].p1;
        }
        else
        {
          if(lcmb[sl].o[0]=='+')  scp[i][j]=line[lcmb[sl].l[0]].p1;
          else                    scp[i][j]=line[lcmb[sl].l[0]].p2;
        }
      }
      else if (surf[s].typ[j]=='l')
      {
        if (surf[s].o[j]=='-')
        {
          scp[i][j]=line[surf[s].l[j]].p2;
        }
        else
        {
          scp[i][j]=line[surf[s].l[j]].p1;
        }
      }
      else { errMsg (" ERROR: in meshBodies surf.typ:%1c not known\n", surf[s].typ[j]); exit(-1);}
    }
#if TEST1
  printf ("Edge-points of Surf:%s ",  surf[s].name );
  for (j=0; j<surf[s].nl; j++)
      printf (" scp[%d][%d]=%d",i,j, scp[i][j]);
  for (j=0; j<surf[s].nl; j++)
      printf (" %s", point[scp[i][j]].name);
  printf ("\n");
#endif
  }

  /* die surfs des 6-seitigen bodies muessen 4 kanten haben!  */
  for(s=0; s<body[b_indx].ns; s++)
  {
    j=body[b_indx].s[s];
    if (surf[j].nl!=4)
    {
      printf(" ERROR: Surf has no %d edges (has:%d)\n", MAX_EDGES_PER_SURF, surf[j].nl );
      err=-1;
      goto noEdges;
    }
  }
  
  /* bestimmung der body-edges  */
  
  /* bcp 0-3 werden durch die Mastersurf (0) festgelegt (orientabhaengig)  */
  if ( body[b_indx].o[0]=='+')
  {
    bcp[0]=scp[0][3];
    bcp[1]=scp[0][2];
    bcp[2]=scp[0][1];
    bcp[3]=scp[0][0];
  }
  else if (body[b_indx].o[0]=='-')
  {
    bcp[0]=scp[0][2];
    bcp[1]=scp[0][3];
    bcp[2]=scp[0][0];
    bcp[3]=scp[0][1];
  }
  else
  {
    errMsg (" ERROR: in meshBodies surface-orientation not known.\n");
    exit(-1);
  }
  
  /* bcp 4-5 werden durch die surf (2) festgelegt (orientabhaengig)  */
  /* suche den mit bcp(0) verbundenen punkt (Verknuepfungspunkt)  */
  for(j=0; j<MAX_EDGES_PER_SURF; j++)
  {
    if (scp[2][j]==bcp[0])
    {
      if ( body[b_indx].o[2]=='+')
      {
        if (j==0)
        {
          bcp[5]=scp[2][2];
          bcp[4]=scp[2][3];
        }
        else if (j==1)
        {
          bcp[5]=scp[2][3];
          bcp[4]=scp[2][0];
        }
        else if (j==2)
        {
          bcp[5]=scp[2][0];
          bcp[4]=scp[2][1];
        }
        else if (j==3)
        {
          bcp[5]=scp[2][1];
          bcp[4]=scp[2][2];
        }
        else
        {
          errMsg (" ERROR: in meshBodies edge not known.\n");
          exit(-1);
        }
      }
      else if (body[b_indx].o[2]=='-')
      {
        if (j==0)
        {
          bcp[5]=scp[2][2];
          bcp[4]=scp[2][1];
        }
        else if (j==1)
        {
          bcp[5]=scp[2][3];
          bcp[4]=scp[2][2];
        }
        else if (j==2)
        {
          bcp[5]=scp[2][0];
          bcp[4]=scp[2][3];
        }
        else if (j==3)
        {
          bcp[5]=scp[2][1];
          bcp[4]=scp[2][0];
        }
        else
        {
          errMsg (" ERROR: in meshBodies edge not known.\n");
          exit(-1);
        }
      }
      else
      {
        errMsg (" ERROR: in meshBodies surface-orientation not known.\n");
        exit(-1);
      }
    }
  }
  
  /* bcp 6-7 werden durch die surf (4) festgelegt (orientabhaengig)  */
  /* suche den mit bcp(3) verbundenen punkt (Verknuepfungspunkt)  */
  for(j=0; j<MAX_EDGES_PER_SURF; j++)
  {
    if (scp[4][j]==bcp[3])
    {
      if ( body[b_indx].o[4]=='-')
      {
        if (j==0)
        {
          bcp[6]=scp[4][2];
          bcp[7]=scp[4][3];
        }
        else if (j==1)
        {
          bcp[6]=scp[4][3];
          bcp[7]=scp[4][0];
        }
        else if (j==2)
        {
          bcp[6]=scp[4][0];
          bcp[7]=scp[4][1];
        }
        else if (j==3)
        {
          bcp[6]=scp[4][1];
          bcp[7]=scp[4][2];
        }
        else
        {
          errMsg (" ERROR: in meshBodies edge not known.\n");
          exit(-1);
        }
      }
      else if (body[b_indx].o[4]=='+')
      {
        if (j==0)
        {
          bcp[6]=scp[4][2];
          bcp[7]=scp[4][1];
        }
        else if (j==1)
        {
          bcp[6]=scp[4][3];
          bcp[7]=scp[4][2];
        }
        else if (j==2)
        {
          bcp[6]=scp[4][0];
          bcp[7]=scp[4][3];
        }
        else if (j==3)
        {
          bcp[6]=scp[4][1];
          bcp[7]=scp[4][0];
        }
        else
        {
          errMsg (" ERROR: in meshBodies edge not known.\n");
          exit(-1);
        }
      }
      else
      {
        errMsg (" ERROR: in meshBodies surface-orientation not known.\n");
        exit(-1);
      }
    }
  }
  
  /* bestimmung der Referenzpunkte der Surfaces  */
  /* srefp[0]=cp-index der Bodysurf:0 der mit bcp[0] zussammenfaellt.  */
  /* srefp[1]=cp-index der Bodysurf:1 der mit bcp[7] zussammenfaellt.  */
  /* srefp[2]=cp-index der Bodysurf:2 der mit bcp[0] zussammenfaellt.  */
  /* srefp[3]=cp-index der Bodysurf:3 der mit bcp[0] zussammenfaellt.  */
  /* srefp[4]=cp-index der Bodysurf:4 der mit bcp[3] zussammenfaellt.  */
  /* srefp[5]=cp-index der Bodysurf:5 der mit bcp[1] zussammenfaellt.  */
  for(j=0; j<MAX_EDGES_PER_SURF; j++) srefp[j]=-1;
  for(j=0; j<MAX_EDGES_PER_SURF; j++)
  {
    if (scp[0][j]==bcp[0]) srefp[0]=j;
    if (scp[2][j]==bcp[0]) srefp[2]=j;
    if (scp[3][j]==bcp[0]) srefp[3]=j;
    if (scp[4][j]==bcp[3]) srefp[4]=j;
    if (scp[5][j]==bcp[1]) srefp[5]=j;
  }
  /* Bodysurf 1 ist ein sonderfall. Diese Surface kann zu einer Linie oder einem Punkt */
  /* kollabiert sein. In diesem Fall muessen zwei aufeinander folgende Punkte abgeprueft*/
  /* werden (diese bilden eine Kante). Die gesuchte Kante bildet die "u"-Achse und     */
  /* laeuft fuer die + orientierte surface von bcp7 zu bcp4. */
  srefp[1]=-1;
  if (body[b_indx].o[1]=='+')
  {
      if ((scp[1][0]==bcp[7])&&(scp[1][1]==bcp[4])) srefp[1]=0;
      if ((scp[1][1]==bcp[7])&&(scp[1][2]==bcp[4])) srefp[1]=1;
      if ((scp[1][2]==bcp[7])&&(scp[1][3]==bcp[4])) srefp[1]=2;
      if ((scp[1][3]==bcp[7])&&(scp[1][0]==bcp[4])) srefp[1]=3;
  }
  else /* surf is - oriented */
  {
      if ((scp[1][0]==bcp[7])&&(scp[1][1]==bcp[6])) srefp[1]=0;
      if ((scp[1][1]==bcp[7])&&(scp[1][2]==bcp[6])) srefp[1]=1;
      if ((scp[1][2]==bcp[7])&&(scp[1][3]==bcp[6])) srefp[1]=2;
      if ((scp[1][3]==bcp[7])&&(scp[1][0]==bcp[6])) srefp[1]=3;
  }
  for(j=0; j<6;j++) if(srefp[j]<0)
  {
    errMsg(" ERROR: in meshBodies edge:%d not known:%d, body:%s\n", j, srefp[j], body[b_indx].name );
    err=-1;
    goto noEdges;
  }

#if TEST1
  for(j=0; j<6;j++)
    printf ("surf[%d]:%s refpnt:%s edge:%d\n",j, surf[ body[b_indx].s[j] ].name, point[ (scp[j][srefp[j]]) ].name, srefp[j]);
#endif
  err=1;
  noEdges:;
  for(i=0; i<body[b_indx].ns; i++) free(scp[i]);
  free(scp);
  return(err);
}

/**********************************************************************************/
/*                                                                                */
/* creates temporary-nodes and final elements in all bodies                       */
/*                                                                                */
/**********************************************************************************/
int meshBodies( int setNr )
{
  int noBodyMesh=0, mapflag;
  int i,j=0,k,n,s,  u,v,w, a,b=0,c;
  int umax,vmax,wmax, b_indx, anz_b;
  int   en[26];
  static int   *n_uvw=(int *)NULL;
  static int   *n_abc=(int *)NULL;
  static double *x=(double *)NULL;
  static double *y=(double *)NULL;
  static double *z=(double *)NULL;
  static int **div_l=NULL;  /* divisions of the surf-edges, 7== actual max-number of surfaces */

  int bod;
  int bcp[MAX_CORNERS_PER_BODY];               /* corner points of the body */
  int srefp[MAX_SURFS_PER_BODY];               /* Reference-point of the Surface to the body */

  int unbalance[MAX_SURFS_PER_BODY];
  
  int amax,bmax,cmax, offs_sa1, offs_sa2, msur, div_sa1, div_sa2;

  /* variables for the mesh-improver */
  static int   nj, *n_indx=NULL, *n_ori=NULL, **e_nod=NULL, *n_type=NULL, **n_edge=NULL, **s_div=NULL;
  static double **n_coord=NULL;

  /* and for cfd-mesh */
  int ii, jj;
  char surFlag;
  int bcp2[MAX_CORNERS_PER_BODY];               /* corner points of the corresponding body */
  int bcp3[MAX_CORNERS_PER_BODY];               /* corner points of the corresponding body */
  int div_lu[6][4];
  int imax, jmax, kmax;
  int iislave=0;

  /* div_l must be (int **) */ 
  if(div_l==NULL)
  {
    if((div_l=(int **)malloc((int)(MAX_SURFS_PER_BODY+2)*sizeof(int *)))==NULL)
    { printf(" ERROR: realloc failure in meshBodies body:%s can not be meshed\n\n", body[b].name); return(-1); }
    for(j=0; j<(MAX_SURFS_PER_BODY+1); j++)
    {
      if((div_l[j]=(int *)malloc((int)(MAX_EDGES_PER_SURF+1)*sizeof(int)))==NULL)
      { printf(" ERROR: realloc failure in meshBodies body:%s can not be meshed\n\n", body[b].name); return(-1); }
    }
  }

  /* save the amount of bodies to mesh */
  anz_b=set[setNr].anz_b;

  for (bod=0; bod<anz_b; bod++)
  {
    b_indx=set[setNr].body[bod];

    /* check if all surfs defining the body were successfully meshed before. */
    for(i=0; i<body[b_indx].ns; i++)
    {
      if(surf[body[b_indx].s[i]].fail==1)
        { printf("ERROR: surf:%s of body:%s not meshed\n",surf[body[b_indx].s[i]].name,body[b_indx].name); goto noEtypDefined; }
    }

    if ((body[b_indx].etyp==1)||(body[b_indx].etyp==4))
    {
      body[b_indx].fail=1;
      if(printFlag) printf ("meshing body:%s with %d surfs\n", body[b_indx].name, body[b_indx].ns);
    }
    else
      goto nextBody;

    /* check how much surfs define the body. If not 6 then create a substitute-body with 6 surfs */
    if(body[b_indx].ns==5)
    {
      if(bodyFrom5Surfs(&b_indx)==-1) goto noEtypDefined;
      mapflag=1;
    }
    else if(body[b_indx].ns==7)
    {
      if(bodyFrom7Surfs(&b_indx)==-1) goto noEtypDefined;
      mapflag=1;
    }
    else if (body[b_indx].ns!=6)
    {
      errMsg (" ERROR: in meshBodies, body:%s has %d surfs but must have 5 to 7!\n", body[b_indx].name, body[b_indx].ns);
      goto noEtypDefined;
    }
    else  mapflag=0;

    /* suche die Eckpunkte der surfaces, setzt einen orientierten body voraus. */
    /* so das die punktefolge der linienfolge entspricht */
    if( generateBodyCornerPoints( b_indx, bcp, srefp) == -1) goto noEtypDefined;

    /* liste die gefundenen Eckpunkte des bodies  */
#if TEST1
  printf ("Edgepoints of Body:%s ",  body[b_indx].name );
  for (j=0; j<MAX_CORNERS_PER_BODY; j++)
    printf (" %s", point[bcp[j]].name);
  printf ("\n");
#endif


    /* Bodytopologie: bestimme die parametrischen Kantenlaengen des bodies        */
    /* Der Nullpunkt liegt im Schnittpunkt von surf 0 2 3 (surfs starten mit 0)  */
    /* Die u-Achse liegt in der schnittlinie von 2 3  */
    /* Die v-Achse liegt in der schnittlinie von 0 2  */
    /* Die w-Achse liegt in der schnittlinie von 0 3  */

    /* Bestimme die divisions der 6 bodyflaechen zur erkennung von unbalance und */
    /* bestimme die parametrischen Kantenlaengen des bodies umax, vmax, wmax mit s2 und s3 */
    /* fuer den fall das kein unbalance erkannt wurde */

    n=0;
    for(j=0; j<MAX_SURFS_PER_BODY;j++)
    {
      unbalance[j]= getSurfDivs( b_indx, j, &div_l[j][0] );
      if (unbalance[j]==-1) goto noEtypDefined;
      n+=unbalance[j];
    }


    /* unbalanced body-surfs */
    if(n==2)
    {
      /* surfs are not all balanced */
      /* check if it is still meshable */

      /* only two surfs must be unbalanced and must be opposite */
      if( fillBody2( b_indx, srefp, div_l, unbalance, &n_abc, &amax, &bmax, &cmax, &offs_sa1, &offs_sa2, &msur, &div_sa1, &div_sa2 )==-1) goto noEtypDefined;
      body[b_indx].fail=0;

      /* allocate memory for embeded elements */
      if((body[b_indx].elem=(int *)realloc((int *)body[b_indx].elem, (amax*bmax*cmax)*sizeof(int)) )==NULL)
      { printf(" ERROR: realloc failure in meshBodies body:%s can not be meshed\n\n"
        , body[b_indx].name); return(-1); }
      
      /* START of section mesh-improver, allocate memory for the mesh-improver */
      if((n_indx=(int *)realloc((int *)n_indx, (apre->nmax+1)*sizeof(int)) )==NULL)
      { printf(" ERROR: n_indx realloc failure in meshBodies body:%s can not be meshed\n\n"
        , body[b_indx].name); return(-1); }
      if((n_ori=(int *)realloc((int *)n_ori, (amax*bmax*cmax)*sizeof(int)) )==NULL)
      { printf(" ERROR: n_ori realloc failure in meshBodies body:%s can not be meshed\n\n"
        , body[b_indx].name); return(-1); }
      if((e_nod=(int **)realloc((int **)e_nod, (amax*bmax*cmax)*sizeof(int *)) )==NULL)
      { printf(" ERROR: e_nod realloc failure in meshBodies body:%s can not be meshed\n\n"
        , body[b_indx].name); return(-1); }
      for (i=0; i<(amax*bmax*cmax); i++)
        if((e_nod[i]=(int *)malloc( (int)20*sizeof(int)) )==NULL)
        { printf(" ERROR: e_nod[i] malloc failure in meshBodies body:%s can not be meshed\n\n"
          , body[b_indx].name); return(-1); }
      if((n_edge=(int **)realloc((int **)n_edge, (int)MAX_SURFS_PER_BODY*sizeof(int *)) )==NULL)
      { printf(" ERROR: n_edge realloc failure in meshBodies body:%s can not be meshed\n\n"
        , body[b_indx].name); return(-1); }
      for (i=0; i<MAX_SURFS_PER_BODY; i++)
        if((n_edge[i]=(int *)malloc( (int)(amax*bmax*cmax)*sizeof(int)) )==NULL)
        { printf(" ERROR: n_edge[%d] malloc failure in meshBodies body:%s can not be meshed\n\n"
          , i, body[b_indx].name); return(-1); }
      if((s_div=(int **)realloc((int **)s_div, (int)MAX_SURFS_PER_BODY*sizeof(int *)) )==NULL)
      { printf(" ERROR: s_div realloc failure in meshBodies body:%s can not be meshed\n\n"
        , body[b_indx].name); return(-1); }
      for (i=0; i<MAX_SURFS_PER_BODY; i++)
        if((s_div[i]=(int *)malloc( (int)MAX_EDGES_PER_SURF*sizeof(int)) )==NULL)
        { printf(" ERROR: s_div[i] malloc failure in meshBodies body:%s can not be meshed\n\n"
          , body[b_indx].name); return(-1); }
      if((n_type=(int *)realloc((int *)n_type, (amax*bmax*cmax+1)*sizeof(int)) )==NULL)
      { printf(" ERROR: n_type realloc failure in meshBodies body:%s can not be meshed\n\n"
        , body[b_indx].name); return(-1); }
      if((n_coord=(double **)realloc((double **)n_coord, (int)(amax*bmax*cmax)*sizeof(double *)) )==NULL)
      { printf(" ERROR: n_coord realloc failure in meshBodies body:%s can not be meshed\n\n"
        , body[b_indx].name); return(-1); }
      for (i=0; i<(amax*bmax*cmax); i++)
        if((n_coord[i]=(double *)malloc( (int)3*sizeof(double)) )==NULL)
        { printf(" ERROR: n_coord[%d] malloc failure in meshBodies body:%s can not be meshed\n\n"
          , i, body[b_indx].name); return(-1); }
      
      /* ini data for mesh-improver */
      j=0;
      for(i=0; i<=apre->nmax; i++) n_indx[i]=-1;      /* initialized as unused */
      for(i=0; i<(amax*bmax*cmax); i++) n_type[i]=0; /* initialized as bulk nodes */ 
      /* INTERRUPT of section mesh-improver */


      /* allocate memory for final-node-buffer nbuf and the final nodes */
      if ((nbuf = (int **)realloc((int **)nbuf, (apre->nmax+1)*sizeof(int *)) ) == NULL )
      { printf(" ERROR: realloc failure in meshSurf, nodes not installed\n\n"); return(-1); }
      for (v=sum_nbuf; v<=apre->nmax; v++)
      {
        if ((nbuf[v] = (int *)malloc( (2)*sizeof(int)) ) == NULL )
        { printf(" ERROR: realloc failure in meshSurf, nodes not installed\n\n"); return(-1); }
        nbuf[v][0]=0;
      }
      sum_nbuf=apre->nmax+1;


      /* erzeugen der elemente  */
      k=0;
      if (body[b_indx].etyp==1)
      {
        for (c=0; c<cmax-1; c++)
        {
          for (a=0; a<amax-1; a++)
          {
            for (b=0; b<bmax-1; b++)
            {
              if ((a>=div_sa1)&&(b<offs_sa1)) goto noelem1a;
              if ((a>=div_sa2)&&(b>=bmax-1-offs_sa2)) goto noelem1a;
              en[0]=n_abc[(a  )*bmax*cmax + b     *cmax    +c];          
              en[1]=n_abc[(a+1)*bmax*cmax + b     *cmax    +c];          
              en[2]=n_abc[(a+1)*bmax*cmax + (b+1) *cmax    +c];          
              en[3]=n_abc[(a  )*bmax*cmax + (b+1) *cmax    +c];          
              en[4]=n_abc[(a  )*bmax*cmax + b     *cmax    +(c+1)];          
              en[5]=n_abc[(a+1)*bmax*cmax + b     *cmax    +(c+1)];          
              en[6]=n_abc[(a+1)*bmax*cmax + (b+1) *cmax    +(c+1)];          
              en[7]=n_abc[(a  )*bmax*cmax + (b+1) *cmax    +(c+1)];          
#if TEST
              printf(" e(%d,%d,%d):%d -> e(%d,%d,%d):%d\n",a,b,c,en[0],a+1,b+1,c+1,en[6]);
              printf(" en1: %d %d %d %d \n",en[0],en[1],en[2],en[3]); 
              printf(" en2: %d %d %d %d \n",en[4],en[5],en[6],en[7]); 
#endif
              elem_define( anz->emax+1, 1, en, 0, body[b_indx].eattr );
              body[b_indx].elem[k]=anz->emax;

              /* RESTART of section mesh-improver, describe variables for the mesh-improver */
              for (i=0; i<8; i++)
              {
                if( n_indx[en[i]]==-1) /* node not stored yet */
                {
                  n_ori[j]=en[i];
                  n_coord[j][0]=npre[n_ori[j]].nx;
                  n_coord[j][1]=npre[n_ori[j]].ny;
                  n_coord[j][2]=npre[n_ori[j]].nz;
                  j++;
                  n_indx[en[i]]=j; /* first number is "1" to match the needs of the improver */
                }
#if TEST
		printf("en[i]:%d n_indx[en[i]]:%d k:%d i:%d\n", en[i],n_indx[en[i]],k,i);
#endif
                e_nod[k][i]=n_indx[en[i]];
              }
              /* INTERRUPT of section mesh-improver */

              k++;
              noelem1a:;
            }
          }
        }
      }      
      else if (body[b_indx].etyp==4)
      {
        for (c=0; c<cmax-1; c+=2)
        {
          for (a=0; a<amax-1; a+=2)
          {
            for (b=0; b<bmax-1; b+=2)
            {
              if ((a>=div_sa1)&&(b<offs_sa1)) goto noelem4a;
              if ((a>=div_sa2)&&(b>=bmax-1-offs_sa2)) goto noelem4a;
              en[0]=n_abc[(a  )*bmax*cmax + b     *cmax    +c];          
              en[1]=n_abc[(a+2)*bmax*cmax + b     *cmax    +c];          
              en[2]=n_abc[(a+2)*bmax*cmax + (b+2) *cmax    +c];          
              en[3]=n_abc[(a  )*bmax*cmax + (b+2) *cmax    +c];          
              en[4]=n_abc[(a  )*bmax*cmax + b     *cmax    +(c+2)];          
              en[5]=n_abc[(a+2)*bmax*cmax + b     *cmax    +(c+2)];          
              en[6]=n_abc[(a+2)*bmax*cmax + (b+2) *cmax    +(c+2)];          
              en[7]=n_abc[(a  )*bmax*cmax + (b+2) *cmax    +(c+2)];          

              en[8]=n_abc[(a+1)*bmax*cmax + b     *cmax    +c];          
              en[9]=n_abc[(a+2)*bmax*cmax + (b+1) *cmax    +c];          
              en[10]=n_abc[(a+1)*bmax*cmax + (b+2) *cmax    +c];          
              en[11]=n_abc[(a  )*bmax*cmax + (b+1) *cmax    +c];          
              en[12]=n_abc[(a  )*bmax*cmax + b     *cmax    +(c+1)];          
              en[13]=n_abc[(a+2)*bmax*cmax + b     *cmax    +(c+1)];          
              en[14]=n_abc[(a+2)*bmax*cmax + (b+2) *cmax    +(c+1)];          
              en[15]=n_abc[(a  )*bmax*cmax + (b+2) *cmax    +(c+1)];
          
              en[16]=n_abc[(a+1)*bmax*cmax + b     *cmax    +(c+2)];          
              en[17]=n_abc[(a+2)*bmax*cmax + (b+1) *cmax    +(c+2)];          
              en[18]=n_abc[(a+1)*bmax*cmax + (b+2) *cmax    +(c+2)];          
              en[19]=n_abc[(a  )*bmax*cmax + (b+1) *cmax    +(c+2)];          
#if TEST
              printf(" e(%d,%d,%d):%d -> e(%d,%d,%d):%d\n"
              ,a,b,c,en[0],a+2,b+2,c+2,en[6]);  
              printf(" en1: %d %d %d %d \n",en[0],en[1],en[2],en[3]); 
              printf(" en2: %d %d %d %d \n",en[4],en[5],en[6],en[7]); 
              printf(" en3: %d %d %d %d \n",en[8],en[9],en[10],en[11]); 
              printf(" en4: %d %d %d %d \n",en[12],en[13],en[14],en[15]); 
              printf(" en4: %d %d %d %d \n",en[16],en[17],en[18],en[19]); 
#endif
              elem_define(anz->emax+1, 4, en, 0, body[b_indx].eattr );
              body[b_indx].elem[k]=anz->emax;

              /* RESTART of section mesh-improver, describe variables for the mesh-improver */
              for (i=0; i<20; i++)
              {
                if( n_indx[en[i]]==-1) /* node not stored yet */
                {
                  n_ori[j]=en[i];
                  n_coord[j][0]=npre[n_ori[j]].nx;
                  n_coord[j][1]=npre[n_ori[j]].ny;
                  n_coord[j][2]=npre[n_ori[j]].nz;
                  j++;
                  n_indx[en[i]]=j; /* first number is "1" to match the needs of the improver */
                }
#if TEST
		printf("en[i]:%d n_indx[en[i]]:%d k:%d i:%d\n", en[i],n_indx[en[i]],k,i);
#endif
                e_nod[k][i]=n_indx[en[i]];
              }
              /* INTERRUPT of section mesh-improver */

              k++;
              noelem4a:;
            }
          }
        }
      }
      body[b_indx].ne=k;

      nj=j;

      /* RESTART of section mesh-improver: determine the surface and edge nodes */
      /* at first the surfs with unbalanced div */

      /* side0 only surfs*/
      c=0;
      for (a=0; a<amax; a++)
      {
        for (b=0; b<bmax; b++)
        {
          if ((a>=div_sa1)&&(b<offs_sa1)) ;
          else if ((a>=div_sa2)&&(b>bmax-1-offs_sa2)) ;
          else
          {
            s=n_abc[ a *bmax*cmax + b     *cmax    +c]; 
            if (n_indx[s]>-1) n_type[n_indx[s]]=1;
          }
        }
      }
      /* side1 only surfs*/
      c=cmax-1;
      for (a=0; a<amax; a++)
      {
        for (b=0; b<bmax; b++)
        {
          if ((a>=div_sa1)&&(b<offs_sa1)) ;
          else if ((a>=div_sa2)&&(b>bmax-1-offs_sa2)) ;
          else
          {
            s=n_abc[ a *bmax*cmax + b     *cmax    +c];
            if (n_indx[s]>-1) n_type[n_indx[s]]=2;
          }
        }
      }
      /* side2 */
      a=0;
      for (c=0; c<cmax; c++)
      {
        for (b=0; b<bmax; b++)
        {
          s=n_abc[ a *bmax*cmax + b     *cmax    +c];
          if (n_indx[s]>-1) 
          {
            if(c==0) n_type[n_indx[s]]=-2;
	    else if(c==cmax-1) n_type[n_indx[s]]=-3;
            else if((b==0)||(b==bmax-1))      n_type[n_indx[s]]=-1;
            else n_type[n_indx[s]]=3;
          }
        }
      }
      /* side3 */
      b=0;
      for (a=0; a<amax-offs_sa1; a++)
      {
        for (c=0; c<cmax; c++)
        {
          s=n_abc[ a *bmax*cmax + b     *cmax    +c];
          if (n_indx[s]>-1) 
          {
            if(c==0) n_type[n_indx[s]]=-2;
	    else if(c==cmax-1) n_type[n_indx[s]]=-3;
            else if((a==0)||(a==amax-1-offs_sa1)) n_type[n_indx[s]]=-1;
            else n_type[n_indx[s]]=4;
          }
        }
      }
      /* side4 */
      a=amax-1;
      for (b=offs_sa1; b<bmax-offs_sa2; b++)
      {
        for (c=0; c<cmax; c++)
        {
          s=n_abc[ a *bmax*cmax + b     *cmax    +c];
          if (n_indx[s]>-1) 
          {
            if(c==0) n_type[n_indx[s]]=-2;
	    else if(c==cmax-1) n_type[n_indx[s]]=-3;
            else if((b==offs_sa1)||(b==bmax-1-offs_sa2)) n_type[n_indx[s]]=-1; 
            else n_type[n_indx[s]]=5;
          }
        }
      }
      /* side5 */
      b=bmax-1;
      for (a=0; a<amax-offs_sa2; a++)
      {
        for (c=0; c<cmax; c++)
        {
          s=n_abc[ a *bmax*cmax + b     *cmax    +c];
          if (n_indx[s]>-1) 
          {
            if(c==0) n_type[n_indx[s]]=-2;
	    else if(c==cmax-1) n_type[n_indx[s]]=-3;
            else if((a==0)||(a==amax-1-offs_sa2)) n_type[n_indx[s]]=-1;
            else n_type[n_indx[s]]=6;
          }
        }
      }
      /* for (j=0; j<nj; j++) printf(" n_type[j+1]:%d n_ori[j]:%d \n", n_type[j+1], n_ori[j]); */

      /* compiling the edge nodes */
      /* side 0 */
      v=0;
      u=0;
      a=0, c=0; 
      for (b=0; b<bmax-1; b++) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][0]=b;
      b=bmax-1; 
      for (a=0; a<amax-1-offs_sa2; a++) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][1]=a;
      a=amax-1; 
      for (b=bmax-1-offs_sa2; b>offs_sa1; b--) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][2]=bmax-1-offs_sa2-offs_sa1;
      b=0; 
      for (a=amax-1-offs_sa1; a>0; a--) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][3]=amax-1-offs_sa1;

      /* side 1 */
      v=1;
      u=0;
      b=0, c=cmax-1; 
      for (a=0; a<amax-1-offs_sa1; a++) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][0]=a;
      a=amax-1; 
      for (b=offs_sa1; b<bmax-1-offs_sa2; b++) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][1]=b-offs_sa1;
      b=bmax-1; 
      for (a=amax-1-offs_sa2; a>0; a--) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][2]=amax-1-offs_sa2;
      a=0; 
      for (b=bmax-1; b>0; b--) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][3]=bmax-1;
      /*
	for (a=0; a<u; a++) printf("edge[%d][%d]:%d\n", v,a, n_edge[v][a]);
	for (a=0; a<4; a++) printf("s_div[%d][%d]:%d\n", v,a, s_div[v][a]);
	*/

      /* side 2 */
      v=2;
      u=0;
      a=b=0; 
      for (c=0; c<cmax-1; c++) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][0]=c;
      c=cmax-1; 
      for (b=0; b<bmax-1; b++) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][1]=b;
      b=bmax-1; 
      for (c=cmax-1; c>0; c--) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][2]=cmax-1;
      c=0; 
      for (b=bmax-1; b>0; b--) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][3]=bmax-1;

      /* side 3 */
      v=3;
      u=0;
      c=b=0; 
      for (a=0; a<amax-1-offs_sa1; a++) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][0]=a;
      a=amax-1-offs_sa1;
      for (c=0; c<cmax-1; c++) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][1]=c;
      c=cmax-1; 
      for (a=amax-1-offs_sa1; a>0; a--) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][2]=amax-1-offs_sa1;
      a=0; 
      for (c=cmax-1; c>0; c--) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][3]=cmax-1;

      /* side 4 */
      v=4;
      u=0;
      a=amax-1; c=0;
      for (b=offs_sa1; b<bmax-1-offs_sa2; b++) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][0]=b-offs_sa1;
      b=bmax-1-offs_sa2;
      for (c=0; c<cmax-1; c++) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][1]=c;
      c=cmax-1; 
      for (b=bmax-1-offs_sa2; b>offs_sa1; b--) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][2]=bmax-1-offs_sa2-offs_sa1;
      b=offs_sa1;
      for (c=cmax-1; c>0; c--) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][3]=cmax-1;

      /* side 5 */
      v=5;
      u=0;
      c=cmax-1; b=bmax-1;
      for (a=0; a<amax-1-offs_sa2; a++) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][0]=a;
      a=amax-1-offs_sa2;
      for (c=cmax-1; c>0; c--) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][1]=cmax-1;
      c=0; 
      for (a=amax-1-offs_sa2; a>0; a--) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][2]=amax-1-offs_sa2;
      a=0;
      for (c=0; c<cmax-1; c++) n_edge[v][u++]=n_indx[n_abc[ a *bmax*cmax + b *cmax +c]]; 
      s_div[v][3]=c;

      if(meshImprover( &body[b_indx].etyp, &nj, &k, n_indx, n_ori, n_coord, e_nod, n_type, n_edge, s_div )==0)
      {
        /* write the coordinates back */
        for (i=0; i<nj; i++)
        {
          npre[n_ori[i]].nx=n_coord[i][0];
          npre[n_ori[i]].ny=n_coord[i][1];
          npre[n_ori[i]].nz=n_coord[i][2];
        }
      }

      /* free some space */
      for (i=0; i<(amax*bmax*cmax); i++) free(n_coord[i]);	
      for (i=0; i<(amax*bmax*cmax); i++) free(e_nod[i]); 
      for (i=0; i<MAX_SURFS_PER_BODY; i++) free(n_edge[i]);
      for (i=0; i<MAX_SURFS_PER_BODY; i++) free(s_div[i]);
      /* END of section mesh-improver */
	
    }

    /* body-surfs have balanced edges */
    else if (n==0)
    {
      /* surfs are all balanced, determine umax etc. */
      s=0;
      if (body[b_indx].o[s]=='+')
      {
        if ((srefp[s]==0)||(srefp[s]==2))
        {
          wmax=div_l[s][0]+1;
          vmax=div_l[s][1]+1;
        }
        else if ((srefp[s]==1)||(srefp[s]==3))
        {
          wmax=div_l[s][1]+1;
          vmax=div_l[s][0]+1;
        }
        else
        {
          errMsg("    ERROR: in meshBodies edge%d not known:%d\n",s, srefp[s]);
          exit(-1);
        }
      }
      else
      {
        if ((srefp[s]==0)||(srefp[s]==2))
        {
          wmax=div_l[s][1]+1;
          vmax=div_l[s][0]+1;
        }
        else if ((srefp[s]==1)||(srefp[s]==3))
        {
          wmax=div_l[s][0]+1;
          vmax=div_l[s][1]+1;
        }
        else
        {
          errMsg("    ERROR: in meshBodies edge%d not known:%d\n",s, srefp[s]);
          exit(-1);
        }
      }

      /* bestimme die parametrischen Kantenlaengen des bodies, s3 fuer u,w */
      s=3;
      /* n=srefp[3]; ist gleich der LinienNr die die u-achse beschreibt, wenn die ori. der surf '+' ist */
      n=srefp[s]; /* Body-Surf edgenr */
      if (body[b_indx].o[s]=='-')
      {
        if( n==0) n=3;
        else if( n==1) n=0;
        else if( n==2) n=1;
        else if( n==3) n=2;
      }
      umax=div_l[s][n]+1;

      if(printFlag) printf ("Nr of Nodes in 3D umax:%d vmax:%d wmax:%d\n", umax, vmax, wmax );
      if( (n_uvw=(int *)realloc((int *)n_uvw, (umax)*(vmax)*(wmax)*sizeof(int)) ) == NULL )
      { printf(" ERROR: realloc failure in meshBodies body:%s can not be meshed\n\n", body[b_indx].name); return(-1); }
      if( (x=(double *)realloc((double *)x, (umax)*(vmax)*(wmax)*sizeof(double)) ) == NULL )
      { printf(" ERROR: realloc failure in meshBodies body:%s can not be meshed\n\n", body[b_indx].name); return(-1); }
      if( (y=(double *)realloc((double *)y, (umax)*(vmax)*(wmax)*sizeof(double)) ) == NULL )
      { printf(" ERROR: realloc failure in meshBodies body:%s can not be meshed\n\n", body[b_indx].name); return(-1); }
      if( (z=(double *)realloc((double *)z, (umax)*(vmax)*(wmax)*sizeof(double)) ) == NULL )
      { printf(" ERROR: realloc failure in meshBodies body:%s can not be meshed\n\n", body[b_indx].name); return(-1); }
 
      /* allocate memory for embeded nodes */
      if((body[b_indx].nod=(int *)realloc((int *)body[b_indx].nod,((umax-2)*(vmax-2)*(wmax-2))*sizeof(int)))==NULL)
      { printf(" ERROR: realloc failure in meshBodies body:%s can not be meshed\n\n", body[b_indx].name); return(-1); }

      /* Body mit nodes belegen  */
      if( fillBody( b_indx, srefp, umax, vmax, wmax, n_uvw, x,y,z )==-1) goto noEtypDefined;
      body[b_indx].fail=0;

      /* erzeugen der elemente  */
      k=0;

      /* allocate memory for final-node-buffer nbuf and the final nodes */
      if ((nbuf = (int **)realloc((int **)nbuf, (apre->nmax+1)*sizeof(int *)) ) == NULL )
      { printf(" ERROR: realloc failure in meshSurf, nodes not installed\n\n"); return(-1); }
      for (v=sum_nbuf; v<=apre->nmax; v++)
      {
        if ((nbuf[v] = (int *)malloc( (2)*sizeof(int)) ) == NULL )
        { printf(" ERROR: realloc failure in meshSurf, nodes not installed\n\n"); return(-1); }
        nbuf[v][0]=0;
      }
      sum_nbuf=apre->nmax+1;

      if (body[b_indx].etyp==1) /* also for blocked cfd meshes */
      {
        /* allocate memory for embeded elements */
        if((body[b_indx].elem=(int *)realloc((int *)body[b_indx].elem,((umax-1)*(vmax-1)*(wmax-1))*sizeof(int)))==NULL)
        { printf(" ERROR: realloc failure in meshBodies body:%s can not be meshed\n\n", body[b_indx].name);
          return(-1); }

        if(writeCFDflag==1)
        {       
          /* store the node-indexes for block-structured cfd meshes */
          if(body[b_indx].ori!='+')
  	{
            printf("WARNING: for cfd the body:%s must be positive oriented but is negative oriented.\n", body[b_indx].name);
  	}
  
          printf("# body:%s is duns-body:%d\n", body[b_indx].name, b_indx+1);
          if ( (nBlock = (NodeBlocks *)realloc((NodeBlocks *)nBlock, (apre->b+1) * sizeof(NodeBlocks))) == NULL )
            printf("\n\n ERROR: realloc failed, NodeBlocks\n\n") ;
          if ( (nBlock[apre->b].nod = (int *)malloc( (umax*vmax*wmax+1) * sizeof(int))) == NULL )
            printf("\n\n ERROR: malloc failed, NodeBlocks\n\n") ;
          nBlock[apre->b].dim=3;
          nBlock[apre->b].geo=b_indx;
          nBlock[apre->b].i=umax;
          nBlock[apre->b].j=vmax;
          nBlock[apre->b].k=wmax;
          n=0;
          for (w=0; w<wmax; w++)
          {
            for (v=0; v<vmax; v++)
            {
              for (u=0; u<umax; u++)
              {
                nBlock[apre->b].nod[n]=n_uvw[u*vmax*wmax+v*wmax+w];
                n++;
              }
  	    }
  	  }
  
          /* determine the connectivity for cfd-blocks */
          if(body[b_indx].ns!=6)
          {
            printf("PRG_ERROR: found body with no 6 surfaces:%d, call the admin.\n",body[b_indx].ns);
            exit(1);
          }
          for (v=0; v<body[b_indx].ns; v++)
          {
            /* v: side-nr in duns and isaac */
            /* duns-surfs are imin, imax, jmin, jmax, (kmin, kmax) */
            /* ii: cgx surfaces  */
            if(v==0)      ii=0;
            else if(v==1) ii=1;
            else if(v==2) ii=3;
            else if(v==3) ii=5;
            else if(v==4) ii=2;
            else          ii=4;
   
            nBlock[apre->b].neighbor[v]=-1;
            nBlock[apre->b].bcface[v]=-1;
            nBlock[apre->b].map[v][0]=-1;  nBlock[apre->b].map[v][1]=-1; nBlock[apre->b].map[v][2]=-1;
  	
            /* describe the contact face by indexes */
            /* faces related to bcp[0]: ii= 0,2,3   */
            /* faces related to bcp[6]: ii= 1,4,5   */
            /* i==nBlock[].strt1[face][0], j==nBlock[].strt1[face][1], j==nBlock[].strt1[face][2] */
            imax= umax; jmax= vmax; kmax= wmax;
            if(ii==0)
            {
              nBlock[apre->b].strt1[v][0]=1;    nBlock[apre->b].strt1[v][1]=1;    nBlock[apre->b].strt1[v][2]=1; 
              nBlock[apre->b].end_1[v][0]=1;    nBlock[apre->b].end_1[v][1]=jmax; nBlock[apre->b].end_1[v][2]=kmax;
            }
            if(ii==1)
            {
              nBlock[apre->b].strt1[v][0]=imax; nBlock[apre->b].strt1[v][1]=jmax; nBlock[apre->b].strt1[v][2]=kmax;
              nBlock[apre->b].end_1[v][0]=imax; nBlock[apre->b].end_1[v][1]=1;    nBlock[apre->b].end_1[v][2]=1;   
            }
            if(ii==2)
            {
              nBlock[apre->b].strt1[v][0]=1;    nBlock[apre->b].strt1[v][1]=1;    nBlock[apre->b].strt1[v][2]=1; 
              nBlock[apre->b].end_1[v][0]=imax; nBlock[apre->b].end_1[v][1]=jmax; nBlock[apre->b].end_1[v][2]=1; 
            }
            if(ii==3)
            {
              nBlock[apre->b].strt1[v][0]=1;    nBlock[apre->b].strt1[v][1]=1;    nBlock[apre->b].strt1[v][2]=1; 
              nBlock[apre->b].end_1[v][0]=imax; nBlock[apre->b].end_1[v][1]=1;    nBlock[apre->b].end_1[v][2]=kmax;
            }
            if(ii==4)
            {
              nBlock[apre->b].strt1[v][0]=imax; nBlock[apre->b].strt1[v][1]=jmax; nBlock[apre->b].strt1[v][2]=kmax;
              nBlock[apre->b].end_1[v][0]=1;    nBlock[apre->b].end_1[v][1]=1;    nBlock[apre->b].end_1[v][2]=kmax;
            }
            if(ii==5)
            {
              nBlock[apre->b].strt1[v][0]=imax; nBlock[apre->b].strt1[v][1]=jmax; nBlock[apre->b].strt1[v][2]=kmax;
              nBlock[apre->b].end_1[v][0]=1;    nBlock[apre->b].end_1[v][1]=jmax; nBlock[apre->b].end_1[v][2]=1;   
            }
  
            surFlag=1;
  
            /* search the slave surface */
            for (u=0; u<set[setNr].anz_b; u++)
  	    {
              if((u!=b_indx)&&(body[u].name != (char *)NULL)) for (jj=0; jj<body[u].ns; jj++)
  	      {
  
                if (body[b_indx].s[ii]==body[u].s[jj])
  	        {
                  /* corresponding body */
                  surFlag=0;
#if TEST2
                  printf(" b:%s s:%s matches b:%s\n", body[b_indx].name, surf[body[b_indx].s[ii]].name, body[u].name);
#endif	
                  /* determine the corner-points of the corresponding body */
                  if( generateBodyCornerPoints( u, bcp2, srefp) == -1)  goto noEtypDefined;
                  iislave=jj;
#if TEST2
                  printf ("Edgepoints of Body:%s ",  body[u].name );
                  for (j=0; j<MAX_CORNERS_PER_BODY; j++)
                    printf (" j:%d bcp:%d %s", j, bcp2[j], point[bcp2[j]].name);
                  printf ("\n");
                  for (j=0; j<MAX_SURFS_PER_BODY; j++)
                    printf (" surf %d %s", j,surf[body[u].s[j]].name );
                  printf ("\n");
                
                  /* determine the orientation of the corresponding body */
                  printf(" ii:%d iislave:%d\n", ii, iislave);
#endif
                  /* check if the corresponding body is usable for cfd */
                  /* and determine the amount of nodes in each direction of the slave body */
                  for(j=0; j<body[u].ns; j++)
                  {
                    unbalance[j]= getSurfDivs( u, j, &div_lu[j][0] );
                    if (unbalance[j]!=0)  surFlag=1;
                  }
                  goto matchSurfs;
  	        }
  	      }
  	    }
            matchSurfs:;
  
  
            if(!surFlag)
            {
              nBlock[apre->b].neighbor[v]=u;
  
              /* determine the parametric space of the slave body */
              /* suche die Eckpunkte der surfaces, setzt einen orientierten body voraus. */
              /* so das die punktefolge der linienfolge entspricht */
              if( generateBodyCornerPoints( u, bcp3, srefp) == -1)  goto noEtypDefined;
  
              /* surfs are all balanced, determine umax etc. */
              s=0;
              if (body[u].o[s]=='+')
              {
                if ((srefp[s]==0)||(srefp[s]==2))
                {
                  kmax=div_lu[s][0]+1;
                  jmax=div_lu[s][1]+1;
                }
                else if ((srefp[s]==1)||(srefp[s]==3))
                {
                  kmax=div_lu[s][1]+1;
                  jmax=div_lu[s][0]+1;
                }
                else
                {
                  errMsg("    ERROR: in meshBodies edge%d not known:%d\n",s, srefp[s]);
                  exit(-1);
                }
              }
              else
              {
                if ((srefp[s]==0)||(srefp[s]==2))
                {
                  kmax=div_lu[s][1]+1;
                  jmax=div_lu[s][0]+1;
                }
                else if ((srefp[s]==1)||(srefp[s]==3))
                {
                  kmax=div_lu[s][0]+1;
                  jmax=div_lu[s][1]+1;
                }
                else
                {
                  errMsg("    ERROR: in meshBodies edge%d not known:%d\n",s, srefp[s]);
                  exit(-1);
                }
              }
        
              /* bestimme die parametrischen Kantenlaengen des bodies, s3 fuer u,w */
              s=3;
              /* n=srefp[3]; ist gleich der LinienNr die die u-achse beschreibt, wenn die ori. der surf '+' ist */
              n=srefp[s]; /* Body-Surf edgenr */
              if (body[u].o[s]=='-')
              {
                if( n==0) n=3;
                else if( n==1) n=0;
                else if( n==2) n=1;
                else if( n==3) n=2;
              }
              imax=div_lu[s][n]+1;
  
  
              if((ii==0)&&(iislave==0))
              {
                if     (bcp2[0]==bcp[0]) { nBlock[apre->b].map[v][0]=4; nBlock[apre->b].map[v][1]=3; nBlock[apre->b].map[v][2]=2; }
                else if(bcp2[1]==bcp[0]) { nBlock[apre->b].map[v][0]=4; nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=3; }
                else if(bcp2[2]==bcp[0]) { nBlock[apre->b].map[v][0]=4; nBlock[apre->b].map[v][1]=6; nBlock[apre->b].map[v][2]=5; }
                else if(bcp2[3]==bcp[0]) { nBlock[apre->b].map[v][0]=4; nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=6; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==0)&&(iislave==1))
              {
                if     (bcp2[7]==bcp[0]) { nBlock[apre->b].map[v][0]=1; nBlock[apre->b].map[v][1]=6; nBlock[apre->b].map[v][2]=2; }
                else if(bcp2[6]==bcp[0]) { nBlock[apre->b].map[v][0]=1; nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=6; }
                else if(bcp2[5]==bcp[0]) { nBlock[apre->b].map[v][0]=1; nBlock[apre->b].map[v][1]=3; nBlock[apre->b].map[v][2]=5; }
                else if(bcp2[4]==bcp[0]) { nBlock[apre->b].map[v][0]=1; nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=3; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==0)&&(iislave==2))
              {
                if     (bcp2[4]==bcp[0]) { nBlock[apre->b].map[v][0]=6; nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=2; }
                else if(bcp2[5]==bcp[0]) { nBlock[apre->b].map[v][0]=6; nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=4; }
                else if(bcp2[1]==bcp[0]) { nBlock[apre->b].map[v][0]=6; nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=5; }
                else if(bcp2[0]==bcp[0]) { nBlock[apre->b].map[v][0]=6; nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=1; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==0)&&(iislave==3))
              {
                if     (bcp2[0]==bcp[0]) { nBlock[apre->b].map[v][0]=5; nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=3; }
                else if(bcp2[3]==bcp[0]) { nBlock[apre->b].map[v][0]=5; nBlock[apre->b].map[v][1]=6; nBlock[apre->b].map[v][2]=1; }
                else if(bcp2[7]==bcp[0]) { nBlock[apre->b].map[v][0]=5; nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=6; }
                else if(bcp2[4]==bcp[0]) { nBlock[apre->b].map[v][0]=5; nBlock[apre->b].map[v][1]=3; nBlock[apre->b].map[v][2]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==0)&&(iislave==4))
              {
                if     (bcp2[3]==bcp[0]) { nBlock[apre->b].map[v][0]=3; nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=2; }
                else if(bcp2[2]==bcp[0]) { nBlock[apre->b].map[v][0]=3; nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=1; }
                else if(bcp2[6]==bcp[0]) { nBlock[apre->b].map[v][0]=3; nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=5; }
                else if(bcp2[7]==bcp[0]) { nBlock[apre->b].map[v][0]=3; nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==0)&&(iislave==5))
              {
                if     (bcp2[2]==bcp[0]) { nBlock[apre->b].map[v][0]=2; nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=6; }
                else if(bcp2[1]==bcp[0]) { nBlock[apre->b].map[v][0]=2; nBlock[apre->b].map[v][1]=3; nBlock[apre->b].map[v][2]=1; }
                else if(bcp2[5]==bcp[0]) { nBlock[apre->b].map[v][0]=2; nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=3; }
                else if(bcp2[6]==bcp[0]) { nBlock[apre->b].map[v][0]=2; nBlock[apre->b].map[v][1]=6; nBlock[apre->b].map[v][2]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
      
              else if((ii==2)&&(iislave==0))
              {
                if     (bcp2[0]==bcp[0]) { nBlock[apre->b].map[v][2]=4; nBlock[apre->b].map[v][0]=3; nBlock[apre->b].map[v][1]=2; }
                else if(bcp2[1]==bcp[0]) { nBlock[apre->b].map[v][2]=4; nBlock[apre->b].map[v][0]=5; nBlock[apre->b].map[v][1]=3; }
                else if(bcp2[2]==bcp[0]) { nBlock[apre->b].map[v][2]=4; nBlock[apre->b].map[v][0]=6; nBlock[apre->b].map[v][1]=5; }
                else if(bcp2[3]==bcp[0]) { nBlock[apre->b].map[v][2]=4; nBlock[apre->b].map[v][0]=2; nBlock[apre->b].map[v][1]=6; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==2)&&(iislave==1))
              {
                if     (bcp2[7]==bcp[0]) { nBlock[apre->b].map[v][2]=1; nBlock[apre->b].map[v][0]=6; nBlock[apre->b].map[v][1]=2; }
                else if(bcp2[6]==bcp[0]) { nBlock[apre->b].map[v][2]=1; nBlock[apre->b].map[v][0]=5; nBlock[apre->b].map[v][1]=6; }
                else if(bcp2[5]==bcp[0]) { nBlock[apre->b].map[v][2]=1; nBlock[apre->b].map[v][0]=3; nBlock[apre->b].map[v][1]=5; }
                else if(bcp2[4]==bcp[0]) { nBlock[apre->b].map[v][2]=1; nBlock[apre->b].map[v][0]=2; nBlock[apre->b].map[v][1]=3; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==2)&&(iislave==2))
              {
                if     (bcp2[4]==bcp[0]) { nBlock[apre->b].map[v][2]=6; nBlock[apre->b].map[v][0]=4; nBlock[apre->b].map[v][1]=2; }
                else if(bcp2[5]==bcp[0]) { nBlock[apre->b].map[v][2]=6; nBlock[apre->b].map[v][0]=5; nBlock[apre->b].map[v][1]=4; }
                else if(bcp2[1]==bcp[0]) { nBlock[apre->b].map[v][2]=6; nBlock[apre->b].map[v][0]=1; nBlock[apre->b].map[v][1]=5; }
                else if(bcp2[0]==bcp[0]) { nBlock[apre->b].map[v][2]=6; nBlock[apre->b].map[v][0]=2; nBlock[apre->b].map[v][1]=1; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==2)&&(iislave==3))
              {
                if     (bcp2[0]==bcp[0]) { nBlock[apre->b].map[v][2]=5; nBlock[apre->b].map[v][0]=1; nBlock[apre->b].map[v][1]=3; }
                else if(bcp2[3]==bcp[0]) { nBlock[apre->b].map[v][2]=5; nBlock[apre->b].map[v][0]=6; nBlock[apre->b].map[v][1]=1; }
                else if(bcp2[7]==bcp[0]) { nBlock[apre->b].map[v][2]=5; nBlock[apre->b].map[v][0]=4; nBlock[apre->b].map[v][1]=6; }
                else if(bcp2[4]==bcp[0]) { nBlock[apre->b].map[v][2]=5; nBlock[apre->b].map[v][0]=3; nBlock[apre->b].map[v][1]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==2)&&(iislave==4))
              {
                if     (bcp2[3]==bcp[0]) { nBlock[apre->b].map[v][2]=3; nBlock[apre->b].map[v][0]=1; nBlock[apre->b].map[v][1]=2; }
                else if(bcp2[2]==bcp[0]) { nBlock[apre->b].map[v][2]=3; nBlock[apre->b].map[v][0]=5; nBlock[apre->b].map[v][1]=1; }
                else if(bcp2[6]==bcp[0]) { nBlock[apre->b].map[v][2]=3; nBlock[apre->b].map[v][0]=4; nBlock[apre->b].map[v][1]=5; }
                else if(bcp2[7]==bcp[0]) { nBlock[apre->b].map[v][2]=3; nBlock[apre->b].map[v][0]=2; nBlock[apre->b].map[v][1]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==2)&&(iislave==5))
              {
                if     (bcp2[2]==bcp[0]) { nBlock[apre->b].map[v][2]=2; nBlock[apre->b].map[v][0]=1; nBlock[apre->b].map[v][1]=6; }
                else if(bcp2[1]==bcp[0]) { nBlock[apre->b].map[v][2]=2; nBlock[apre->b].map[v][0]=3; nBlock[apre->b].map[v][1]=1; }
                else if(bcp2[5]==bcp[0]) { nBlock[apre->b].map[v][2]=2; nBlock[apre->b].map[v][0]=4; nBlock[apre->b].map[v][1]=3; }
                else if(bcp2[6]==bcp[0]) { nBlock[apre->b].map[v][2]=2; nBlock[apre->b].map[v][0]=6; nBlock[apre->b].map[v][1]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
      
              else if((ii==3)&&(iislave==0))
              {
                if     (bcp2[0]==bcp[0]) { nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=3; nBlock[apre->b].map[v][0]=2; }
                else if(bcp2[1]==bcp[0]) { nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=5; nBlock[apre->b].map[v][0]=3; }
                else if(bcp2[2]==bcp[0]) { nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=6; nBlock[apre->b].map[v][0]=5; }
                else if(bcp2[3]==bcp[0]) { nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=2; nBlock[apre->b].map[v][0]=6; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==3)&&(iislave==1))
              {
                if     (bcp2[7]==bcp[0]) { nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=6; nBlock[apre->b].map[v][0]=2; }
                else if(bcp2[6]==bcp[0]) { nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=5; nBlock[apre->b].map[v][0]=6; }
                else if(bcp2[5]==bcp[0]) { nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=3; nBlock[apre->b].map[v][0]=5; }
                else if(bcp2[4]==bcp[0]) { nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=2; nBlock[apre->b].map[v][0]=3; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==3)&&(iislave==2))
              {
                if     (bcp2[4]==bcp[0]) { nBlock[apre->b].map[v][1]=6; nBlock[apre->b].map[v][2]=4; nBlock[apre->b].map[v][0]=2; }
                else if(bcp2[5]==bcp[0]) { nBlock[apre->b].map[v][1]=6; nBlock[apre->b].map[v][2]=5; nBlock[apre->b].map[v][0]=4; }
                else if(bcp2[1]==bcp[0]) { nBlock[apre->b].map[v][1]=6; nBlock[apre->b].map[v][2]=1; nBlock[apre->b].map[v][0]=5; }
                else if(bcp2[0]==bcp[0]) { nBlock[apre->b].map[v][1]=6; nBlock[apre->b].map[v][2]=2; nBlock[apre->b].map[v][0]=1; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==3)&&(iislave==3))
              {
                if     (bcp2[0]==bcp[0]) { nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=1; nBlock[apre->b].map[v][0]=3; }
                else if(bcp2[3]==bcp[0]) { nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=6; nBlock[apre->b].map[v][0]=1; }
                else if(bcp2[7]==bcp[0]) { nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=4; nBlock[apre->b].map[v][0]=6; }
                else if(bcp2[4]==bcp[0]) { nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=3; nBlock[apre->b].map[v][0]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==3)&&(iislave==4))
              {
                if     (bcp2[3]==bcp[0]) { nBlock[apre->b].map[v][1]=3; nBlock[apre->b].map[v][2]=1; nBlock[apre->b].map[v][0]=2; }
                else if(bcp2[2]==bcp[0]) { nBlock[apre->b].map[v][1]=3; nBlock[apre->b].map[v][2]=5; nBlock[apre->b].map[v][0]=1; }
                else if(bcp2[6]==bcp[0]) { nBlock[apre->b].map[v][1]=3; nBlock[apre->b].map[v][2]=4; nBlock[apre->b].map[v][0]=5; }
                else if(bcp2[7]==bcp[0]) { nBlock[apre->b].map[v][1]=3; nBlock[apre->b].map[v][2]=2; nBlock[apre->b].map[v][0]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==3)&&(iislave==5))
              {
                if     (bcp2[2]==bcp[0]) { nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=1; nBlock[apre->b].map[v][0]=6; }
                else if(bcp2[1]==bcp[0]) { nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=3; nBlock[apre->b].map[v][0]=1; }
                else if(bcp2[5]==bcp[0]) { nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=4; nBlock[apre->b].map[v][0]=3; }
                else if(bcp2[6]==bcp[0]) { nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=6; nBlock[apre->b].map[v][0]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
      
              else if((ii==1)&&(iislave==0))
              {
                if     (bcp2[1]==bcp[6]) { nBlock[apre->b].map[v][0]=1; nBlock[apre->b].map[v][1]=6; nBlock[apre->b].map[v][2]=2; }
                else if(bcp2[2]==bcp[6]) { nBlock[apre->b].map[v][0]=1; nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=3; }
                else if(bcp2[3]==bcp[6]) { nBlock[apre->b].map[v][0]=1; nBlock[apre->b].map[v][1]=3; nBlock[apre->b].map[v][2]=5; }
                else if(bcp2[0]==bcp[6]) { nBlock[apre->b].map[v][0]=1; nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=6; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==1)&&(iislave==1))
              {
                if     (bcp2[6]==bcp[6]) { nBlock[apre->b].map[v][0]=4; nBlock[apre->b].map[v][1]=3; nBlock[apre->b].map[v][2]=2; }
                else if(bcp2[5]==bcp[6]) { nBlock[apre->b].map[v][0]=4; nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=6; }
                else if(bcp2[4]==bcp[6]) { nBlock[apre->b].map[v][0]=4; nBlock[apre->b].map[v][1]=6; nBlock[apre->b].map[v][2]=5; }
                else if(bcp2[7]==bcp[6]) { nBlock[apre->b].map[v][0]=4; nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=3; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==1)&&(iislave==2))
              {
                if     (bcp2[5]==bcp[6]) { nBlock[apre->b].map[v][0]=3; nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=2; }
                else if(bcp2[1]==bcp[6]) { nBlock[apre->b].map[v][0]=3; nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=4; }
                else if(bcp2[0]==bcp[6]) { nBlock[apre->b].map[v][0]=3; nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=5; }
                else if(bcp2[4]==bcp[6]) { nBlock[apre->b].map[v][0]=3; nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=1; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==1)&&(iislave==3))
              {
                if     (bcp2[3]==bcp[6]) { nBlock[apre->b].map[v][0]=2; nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=3; }
                else if(bcp2[7]==bcp[6]) { nBlock[apre->b].map[v][0]=2; nBlock[apre->b].map[v][1]=3; nBlock[apre->b].map[v][2]=1; }
                else if(bcp2[4]==bcp[6]) { nBlock[apre->b].map[v][0]=2; nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=6; }
                else if(bcp2[0]==bcp[6]) { nBlock[apre->b].map[v][0]=2; nBlock[apre->b].map[v][1]=6; nBlock[apre->b].map[v][2]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==1)&&(iislave==4))
              {
                if     (bcp2[2]==bcp[6]) { nBlock[apre->b].map[v][0]=6; nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=2; }
                else if(bcp2[6]==bcp[6]) { nBlock[apre->b].map[v][0]=6; nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=1; }
                else if(bcp2[7]==bcp[6]) { nBlock[apre->b].map[v][0]=6; nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=5; }
                else if(bcp2[3]==bcp[6]) { nBlock[apre->b].map[v][0]=6; nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==1)&&(iislave==5))
              {
                if     (bcp2[1]==bcp[6]) { nBlock[apre->b].map[v][0]=5; nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=6; }
                else if(bcp2[5]==bcp[6]) { nBlock[apre->b].map[v][0]=5; nBlock[apre->b].map[v][1]=6; nBlock[apre->b].map[v][2]=1; }
                else if(bcp2[6]==bcp[6]) { nBlock[apre->b].map[v][0]=5; nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=3; }
                else if(bcp2[2]==bcp[6]) { nBlock[apre->b].map[v][0]=5; nBlock[apre->b].map[v][1]=3; nBlock[apre->b].map[v][2]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
      
             else if((ii==4)&&(iislave==0))
              {
                if     (bcp2[1]==bcp[6]) { nBlock[apre->b].map[v][2]=1; nBlock[apre->b].map[v][0]=6; nBlock[apre->b].map[v][1]=2; }
                else if(bcp2[2]==bcp[6]) { nBlock[apre->b].map[v][2]=1; nBlock[apre->b].map[v][0]=2; nBlock[apre->b].map[v][1]=3; }
                else if(bcp2[3]==bcp[6]) { nBlock[apre->b].map[v][2]=1; nBlock[apre->b].map[v][0]=3; nBlock[apre->b].map[v][1]=5; }
                else if(bcp2[0]==bcp[6]) { nBlock[apre->b].map[v][2]=1; nBlock[apre->b].map[v][0]=5; nBlock[apre->b].map[v][1]=6; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==4)&&(iislave==1))
              {
                if     (bcp2[6]==bcp[6]) { nBlock[apre->b].map[v][2]=4; nBlock[apre->b].map[v][0]=3; nBlock[apre->b].map[v][1]=2; }
                else if(bcp2[5]==bcp[6]) { nBlock[apre->b].map[v][2]=4; nBlock[apre->b].map[v][0]=2; nBlock[apre->b].map[v][1]=6; }
                else if(bcp2[4]==bcp[6]) { nBlock[apre->b].map[v][2]=4; nBlock[apre->b].map[v][0]=6; nBlock[apre->b].map[v][1]=5; }
                else if(bcp2[7]==bcp[6]) { nBlock[apre->b].map[v][2]=4; nBlock[apre->b].map[v][0]=5; nBlock[apre->b].map[v][1]=3; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==4)&&(iislave==2))
              {
                if     (bcp2[5]==bcp[6]) { nBlock[apre->b].map[v][2]=3; nBlock[apre->b].map[v][0]=1; nBlock[apre->b].map[v][1]=2; }
                else if(bcp2[1]==bcp[6]) { nBlock[apre->b].map[v][2]=3; nBlock[apre->b].map[v][0]=2; nBlock[apre->b].map[v][1]=4; }
                else if(bcp2[0]==bcp[6]) { nBlock[apre->b].map[v][2]=3; nBlock[apre->b].map[v][0]=4; nBlock[apre->b].map[v][1]=5; }
                else if(bcp2[4]==bcp[6]) { nBlock[apre->b].map[v][2]=3; nBlock[apre->b].map[v][0]=5; nBlock[apre->b].map[v][1]=1; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==4)&&(iislave==3))
              {
                if     (bcp2[3]==bcp[6]) { nBlock[apre->b].map[v][2]=2; nBlock[apre->b].map[v][0]=4; nBlock[apre->b].map[v][1]=3; }
                else if(bcp2[7]==bcp[6]) { nBlock[apre->b].map[v][2]=2; nBlock[apre->b].map[v][0]=3; nBlock[apre->b].map[v][1]=1; }
                else if(bcp2[4]==bcp[6]) { nBlock[apre->b].map[v][2]=2; nBlock[apre->b].map[v][0]=1; nBlock[apre->b].map[v][1]=6; }
                else if(bcp2[0]==bcp[6]) { nBlock[apre->b].map[v][2]=2; nBlock[apre->b].map[v][0]=6; nBlock[apre->b].map[v][1]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==4)&&(iislave==4))
              {
                if     (bcp2[2]==bcp[6]) { nBlock[apre->b].map[v][2]=6; nBlock[apre->b].map[v][0]=4; nBlock[apre->b].map[v][1]=2; }
                else if(bcp2[6]==bcp[6]) { nBlock[apre->b].map[v][2]=6; nBlock[apre->b].map[v][0]=2; nBlock[apre->b].map[v][1]=1; }
                else if(bcp2[7]==bcp[6]) { nBlock[apre->b].map[v][2]=6; nBlock[apre->b].map[v][0]=1; nBlock[apre->b].map[v][1]=5; }
                else if(bcp2[3]==bcp[6]) { nBlock[apre->b].map[v][2]=6; nBlock[apre->b].map[v][0]=5; nBlock[apre->b].map[v][1]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==4)&&(iislave==5))
              {
                if     (bcp2[1]==bcp[6]) { nBlock[apre->b].map[v][2]=5; nBlock[apre->b].map[v][0]=4; nBlock[apre->b].map[v][1]=6; }
                else if(bcp2[5]==bcp[6]) { nBlock[apre->b].map[v][2]=5; nBlock[apre->b].map[v][0]=6; nBlock[apre->b].map[v][1]=1; }
                else if(bcp2[6]==bcp[6]) { nBlock[apre->b].map[v][2]=5; nBlock[apre->b].map[v][0]=1; nBlock[apre->b].map[v][1]=3; }
                else if(bcp2[2]==bcp[6]) { nBlock[apre->b].map[v][2]=5; nBlock[apre->b].map[v][0]=3; nBlock[apre->b].map[v][1]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
      
             else if((ii==5)&&(iislave==0))
              {
                if     (bcp2[1]==bcp[6]) { nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=6; nBlock[apre->b].map[v][0]=2; }
                else if(bcp2[2]==bcp[6]) { nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=2; nBlock[apre->b].map[v][0]=3; }
                else if(bcp2[3]==bcp[6]) { nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=3; nBlock[apre->b].map[v][0]=5; }
                else if(bcp2[0]==bcp[6]) { nBlock[apre->b].map[v][1]=1; nBlock[apre->b].map[v][2]=5; nBlock[apre->b].map[v][0]=6; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==5)&&(iislave==1))
              {
                if     (bcp2[6]==bcp[6]) { nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=3; nBlock[apre->b].map[v][0]=2; }
                else if(bcp2[5]==bcp[6]) { nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=2; nBlock[apre->b].map[v][0]=6; }
                else if(bcp2[4]==bcp[6]) { nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=6; nBlock[apre->b].map[v][0]=5; }
                else if(bcp2[7]==bcp[6]) { nBlock[apre->b].map[v][1]=4; nBlock[apre->b].map[v][2]=5; nBlock[apre->b].map[v][0]=3; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==5)&&(iislave==2))
              {
                if     (bcp2[5]==bcp[6]) { nBlock[apre->b].map[v][1]=3; nBlock[apre->b].map[v][2]=1; nBlock[apre->b].map[v][0]=2; }
                else if(bcp2[1]==bcp[6]) { nBlock[apre->b].map[v][1]=3; nBlock[apre->b].map[v][2]=2; nBlock[apre->b].map[v][0]=4; }
                else if(bcp2[0]==bcp[6]) { nBlock[apre->b].map[v][1]=3; nBlock[apre->b].map[v][2]=4; nBlock[apre->b].map[v][0]=5; }
                else if(bcp2[4]==bcp[6]) { nBlock[apre->b].map[v][1]=3; nBlock[apre->b].map[v][2]=5; nBlock[apre->b].map[v][0]=1; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==5)&&(iislave==3))
              {
                if     (bcp2[3]==bcp[6]) { nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=4; nBlock[apre->b].map[v][0]=3; }
                else if(bcp2[7]==bcp[6]) { nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=3; nBlock[apre->b].map[v][0]=1; }
                else if(bcp2[4]==bcp[6]) { nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=1; nBlock[apre->b].map[v][0]=6; }
                else if(bcp2[0]==bcp[6]) { nBlock[apre->b].map[v][1]=2; nBlock[apre->b].map[v][2]=6; nBlock[apre->b].map[v][0]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==5)&&(iislave==4))
              {
                if     (bcp2[2]==bcp[6]) { nBlock[apre->b].map[v][1]=6; nBlock[apre->b].map[v][2]=4; nBlock[apre->b].map[v][0]=2; }
                else if(bcp2[6]==bcp[6]) { nBlock[apre->b].map[v][1]=6; nBlock[apre->b].map[v][2]=2; nBlock[apre->b].map[v][0]=1; }
                else if(bcp2[7]==bcp[6]) { nBlock[apre->b].map[v][1]=6; nBlock[apre->b].map[v][2]=1; nBlock[apre->b].map[v][0]=5; }
                else if(bcp2[3]==bcp[6]) { nBlock[apre->b].map[v][1]=6; nBlock[apre->b].map[v][2]=5; nBlock[apre->b].map[v][0]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else if((ii==5)&&(iislave==5))
              {
                if     (bcp2[1]==bcp[6]) { nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=4; nBlock[apre->b].map[v][0]=6; }
                else if(bcp2[5]==bcp[6]) { nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=6; nBlock[apre->b].map[v][0]=1; }
                else if(bcp2[6]==bcp[6]) { nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=1; nBlock[apre->b].map[v][0]=3; }
                else if(bcp2[2]==bcp[6]) { nBlock[apre->b].map[v][1]=5; nBlock[apre->b].map[v][2]=3; nBlock[apre->b].map[v][0]=4; }
                else printf("ERROR in orienting the corresponding body\n");
              }
              else  printf("ERROR: no adjacent body-face found\n");
  
              /* count the block-to-block interfaces for isaac */
              apre->c++;
  
  
              if((iislave==0) && ((ii==0)||(ii==2)||(ii==3)) )
              {
                if     (bcp2[0]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[1]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[2]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=1; 
                }
                else if(bcp2[3]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=1; 
                }
                else printf("ERROR in orienting the corresponding body\n");
              }
              if((iislave==0) && ((ii==1)||(ii==4)||(ii==5)) )
              {
                if     (bcp2[0]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[1]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[2]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=1; 
                }
                else if(bcp2[3]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=1; 
                }
                else printf("ERROR in orienting the corresponding body\n");
              }
  
              if((iislave==1) && ((ii==0)||(ii==2)||(ii==3)) )
              {
                if     (bcp2[4]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[5]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[6]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=1; 
                }
                else if(bcp2[7]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=1; 
                }
                else printf("ERROR in orienting the corresponding body\n");
              }
              if((iislave==1) && ((ii==1)||(ii==4)||(ii==5)) )
              {
                if     (bcp2[4]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[5]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[6]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=1; 
                }
                else if(bcp2[7]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=1; 
                }
                else printf("ERROR in orienting the corresponding body\n");
              }
  
              if((iislave==2) && ((ii==0)||(ii==2)||(ii==3)) )
              {
                if     (bcp2[0]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=1;
                }
                else if(bcp2[1]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=1;
                }
                else if(bcp2[5]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=1; 
                }
                else if(bcp2[4]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=1; 
                }
                else printf("ERROR in orienting the corresponding body\n");
              }
              if((iislave==2) && ((ii==1)||(ii==4)||(ii==5)) )
              {
                if     (bcp2[0]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=1;
                }
                else if(bcp2[1]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=1;
                }
                else if(bcp2[5]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=1; 
                }
                else if(bcp2[4]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=1; 
                }
                else printf("ERROR in orienting the corresponding body\n");
              }
  
              if((iislave==4) && ((ii==0)||(ii==2)||(ii==3)) )
              {
                if     (bcp2[3]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[2]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[6]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=kmax; 
                }
                else if(bcp2[7]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=kmax; 
                }
                else printf("ERROR in orienting the corresponding body\n");
              }
              if((iislave==4) && ((ii==1)||(ii==4)||(ii==5)) )
              {
                if     (bcp2[3]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[2]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[6]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=kmax; 
                }
                else if(bcp2[7]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=kmax; 
                }
                else printf("ERROR in orienting the corresponding body\n");
              }
  
              if((iislave==3) && ((ii==0)||(ii==2)||(ii==3)) )
              {
                if     (bcp2[0]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[4]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[7]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=1; 
                }
                else if(bcp2[3]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=1; 
                }
                else printf("ERROR in orienting the corresponding body\n");
              }
              if((iislave==3) && ((ii==1)||(ii==4)||(ii==5)) )
              {
                if     (bcp2[0]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[4]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[7]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=1; 
                }
                else if(bcp2[3]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=1;    nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=1;    nBlock[apre->b].end_2[v][2]=1; 
                }
                else printf("ERROR in orienting the corresponding body\n");
              }
  
              if((iislave==5) && ((ii==0)||(ii==2)||(ii==3)) )
              {
                if     (bcp2[1]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[5]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[6]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=1; 
                }
                else if(bcp2[2]==bcp[0])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=1; 
                }
                else printf("ERROR in orienting the corresponding body\n");
              }
              if((iislave==5) && ((ii==1)||(ii==4)||(ii==5)) )
              {
                if     (bcp2[1]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[5]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=1;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=kmax;
                }
                else if(bcp2[6]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=imax; nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=1;    nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=1; 
                }
                else if(bcp2[2]==bcp[6])
                {
                  nBlock[apre->b].strt2[v][0]=1;    nBlock[apre->b].strt2[v][1]=jmax; nBlock[apre->b].strt2[v][2]=kmax;
                  nBlock[apre->b].end_2[v][0]=imax; nBlock[apre->b].end_2[v][1]=jmax; nBlock[apre->b].end_2[v][2]=1; 
                }
                else printf("ERROR in orienting the corresponding body\n");
              }
  
  	    }
  
            if(surFlag)
            {
              /* found a free surface */
              printf("# surf:%s is duns-surf:%d \n", surf[body[b_indx].s[ii]].name, anz_cfdSurfs+1 );
              nBlock[apre->b].neighbor[v]=anz_cfdSurfs+1;
              nBlock[apre->b].bcface[v]=ii;
              anz_cfdSurfs++;
  
              /* i==nBlock[].strt1[][0], j==nBlock[].strt1[][1], imin==1, imax==vmax, jmin==1, jmax==umax */
              /* i,j,k must be ascending, therefore they must be newly written! */
              imax= umax; jmax= vmax; kmax= wmax;
              if(ii==0)
              {
                nBlock[apre->b].strt1[v][0]=1;    nBlock[apre->b].strt1[v][1]=1;    nBlock[apre->b].strt1[v][2]=1; 
                nBlock[apre->b].end_1[v][0]=1;    nBlock[apre->b].end_1[v][1]=jmax; nBlock[apre->b].end_1[v][2]=kmax;
              }
              if(ii==1)
              {
                nBlock[apre->b].strt1[v][0]=imax; nBlock[apre->b].strt1[v][1]=1;    nBlock[apre->b].strt1[v][2]=1; 
                nBlock[apre->b].end_1[v][0]=imax; nBlock[apre->b].end_1[v][1]=jmax; nBlock[apre->b].end_1[v][2]=kmax;
              }
              if(ii==2)
              {
                nBlock[apre->b].strt1[v][0]=1;    nBlock[apre->b].strt1[v][1]=1;    nBlock[apre->b].strt1[v][2]=1; 
                nBlock[apre->b].end_1[v][0]=imax; nBlock[apre->b].end_1[v][1]=jmax; nBlock[apre->b].end_1[v][2]=1; 
              }
              if(ii==3)
              {
                nBlock[apre->b].strt1[v][0]=1;    nBlock[apre->b].strt1[v][1]=1;    nBlock[apre->b].strt1[v][2]=1; 
                nBlock[apre->b].end_1[v][0]=imax; nBlock[apre->b].end_1[v][1]=1;    nBlock[apre->b].end_1[v][2]=kmax;
              }
              if(ii==4)
              {
                nBlock[apre->b].strt1[v][0]=1;    nBlock[apre->b].strt1[v][1]=1;    nBlock[apre->b].strt1[v][2]=kmax;
                nBlock[apre->b].end_1[v][0]=imax; nBlock[apre->b].end_1[v][1]=jmax; nBlock[apre->b].end_1[v][2]=kmax;
              }
              if(ii==5)
              {
                nBlock[apre->b].strt1[v][0]=1;    nBlock[apre->b].strt1[v][1]=jmax; nBlock[apre->b].strt1[v][2]=1; 
                nBlock[apre->b].end_1[v][0]=imax; nBlock[apre->b].end_1[v][1]=jmax; nBlock[apre->b].end_1[v][2]=kmax;
              }
  	    }
  	  }
      
          for (w=0; w<wmax; w++)
          {
            for (v=0; v<vmax; v++)
            {
              for (u=0; u<umax; u++)
              {
                nod( anz, &node, 0, anz->nmax+1, npre[n_uvw[u*vmax*wmax+v*wmax+w]].nx, npre[n_uvw[u*vmax*wmax+v*wmax+w]].ny, npre[n_uvw[u*vmax*wmax+v*wmax+w]].nz, 0 );      
                if(nbuf[n_uvw[u*vmax*wmax+v*wmax+w]][0]>0)
                {
                  if ((nbuf[n_uvw[u*vmax*wmax+v*wmax+w]] = (int *)realloc((int *)nbuf[n_uvw[u*vmax*wmax+v*wmax+w]], (nbuf[n_uvw[u*vmax*wmax+v*wmax+w]][0]+2)*sizeof(int)) ) == NULL )
                  { printf(" ERROR: realloc failure in meshSurf, nodes not installed\n\n"); return(-1); }
                }
                nbuf[n_uvw[u*vmax*wmax+v*wmax+w]][0]++; nbuf[n_uvw[u*vmax*wmax+v*wmax+w]][nbuf[n_uvw[u*vmax*wmax+v*wmax+w]][0]]=anz->nmax;
              }
            }
          }
          apre->b++;
        }
        /* end cfd */


        for (w=0; w<wmax-1; w++)
        {
          for (v=0; v<vmax-1; v++)
          {
            for (u=0; u<umax-1; u++)
            {
              en[0]=n_uvw[ u*vmax*wmax     + v*wmax     + w      ];
              en[1]=n_uvw[ u*vmax*wmax     + v*wmax     + (w+1)  ];
              en[2]=n_uvw[ (u+1)*vmax*wmax + v*wmax     + (w+1)  ];
              en[3]=n_uvw[ (u+1)*vmax*wmax + v*wmax     + w      ];
              en[4]=n_uvw[ u*vmax*wmax     + (v+1)*wmax + w      ];
              en[5]=n_uvw[ u*vmax*wmax     + (v+1)*wmax + (w+1)  ];
              en[6]=n_uvw[ (u+1)*vmax*wmax + (v+1)*wmax + (w+1)  ];
              en[7]=n_uvw[ (u+1)*vmax*wmax + (v+1)*wmax + w      ];
              elem_define( anz->emax+1, 1, en, 0, body[b_indx].eattr );
              body[b_indx].elem[k]=anz->emax;
              k++;
            }
          }
        }
      }
  
      else if (body[b_indx].etyp==4)
      {
       /* allocate memory for embeded elements */
       if((body[b_indx].elem=(int *)realloc((int *)body[b_indx].elem,((umax-1)*(vmax-1)*(wmax-1)/8)*sizeof(int)))==NULL)
       { printf(" ERROR: realloc failure in meshBodies body:%s can not be meshed\n\n", body[b_indx].name);
        return(-1); }
       for (w=0; w<wmax-2; w+=2)
       {
        for (v=0; v<vmax-2; v+=2)
        {
          for (u=0; u<umax-2; u+=2)
          {
            en[0]=n_uvw[ (u  )*vmax*wmax + (v  )*wmax + (w  )  ];
            en[1]=n_uvw[ (u  )*vmax*wmax + (v  )*wmax + (w+2)  ];
            en[2]=n_uvw[ (u+2)*vmax*wmax + (v  )*wmax + (w+2)  ];
            en[3]=n_uvw[ (u+2)*vmax*wmax + (v  )*wmax + (w  )  ];
            en[4]=n_uvw[ (u  )*vmax*wmax + (v+2)*wmax + (w  )  ];
            en[5]=n_uvw[ (u  )*vmax*wmax + (v+2)*wmax + (w+2)  ];
            en[6]=n_uvw[ (u+2)*vmax*wmax + (v+2)*wmax + (w+2)  ];
            en[7]=n_uvw[ (u+2)*vmax*wmax + (v+2)*wmax + (w  )  ];
  
            en[8] =n_uvw[ (u  )*vmax*wmax + (v  )*wmax + (w+1)  ];
            en[9] =n_uvw[ (u+1)*vmax*wmax + (v  )*wmax + (w+2)  ];
            en[10]=n_uvw[ (u+2)*vmax*wmax + (v  )*wmax + (w+1)  ];
            en[11]=n_uvw[ (u+1)*vmax*wmax + (v  )*wmax + (w  )  ];
            en[12]=n_uvw[ (u  )*vmax*wmax + (v+1)*wmax + (w  )  ];
            en[13]=n_uvw[ (u  )*vmax*wmax + (v+1)*wmax + (w+2)  ];
            en[14]=n_uvw[ (u+2)*vmax*wmax + (v+1)*wmax + (w+2)  ];
            en[15]=n_uvw[ (u+2)*vmax*wmax + (v+1)*wmax + (w  )  ];
  
            en[16]=n_uvw[ (u  )*vmax*wmax + (v+2)*wmax + (w+1)  ];
            en[17]=n_uvw[ (u+1)*vmax*wmax + (v+2)*wmax + (w+2)  ];
            en[18]=n_uvw[ (u+2)*vmax*wmax + (v+2)*wmax + (w+1)  ];
            en[19]=n_uvw[ (u+1)*vmax*wmax + (v+2)*wmax + (w  )  ];
            en[20]=n_uvw[ (u  )*vmax*wmax + (v+1)*wmax + (w+1)  ];
            en[21]=n_uvw[ (u+1)*vmax*wmax + (v+1)*wmax + (w+2)  ];
            en[22]=n_uvw[ (u+2)*vmax*wmax + (v+1)*wmax + (w+1)  ];
            en[23]=n_uvw[ (u+1)*vmax*wmax + (v+1)*wmax + (w  )  ];
  
            en[24]=n_uvw[ (u+1)*vmax*wmax + (v+0)*wmax + (w+1)  ];
            en[25]=n_uvw[ (u+1)*vmax*wmax + (v+2)*wmax + (w+1)  ];
            elem_define(anz->emax+1, 4, en, 0, body[b_indx].eattr );
            body[b_indx].elem[k]=anz->emax;
            k++;
          }
        }
       }
      }
      body[b_indx].ne=k;
    }
    else 
    {
      /* wrong number of surfs are not balanced */
      /* body is not meshable */
      goto noEtypDefined;
    }

    
    /* if a substitute body was meshed then map the mesh onto the original one */
    if(mapflag)
    {
#if TEST
      printf("MAP MESH TO BODY :%s\n", body[set[setNr].body[bod]].name);
#endif
      s=set[setNr].body[bod];
      if((body[s].elem=(int *)realloc((int *)body[s].elem, (body[b_indx].ne)*sizeof(int)) )==NULL)
      { printf(" ERROR: realloc failure in meshBodies body:%s can not be meshed\n\n", body[s].name); goto noMesh; }
      for(k=0; k<body[b_indx].ne; k++) body[s].elem[k]=body[b_indx].elem[k];
      body[s].ne=body[b_indx].ne;

      if((body[s].nod=(int *)realloc((int *)body[s].nod, (body[b_indx].nn)*sizeof(int)) )==NULL)
      { printf(" ERROR: realloc failure in meshBodies body:%s can not be meshed\n\n", body[s].name); goto noMesh; }
      for(k=0; k<body[b_indx].nn; k++) body[s].nod[k]=body[b_indx].nod[k];
      body[s].nn=body[b_indx].nn;
    }

    goto nextBody;
    noEtypDefined:;
    pre_seta(specialset->nomesh, "b", body[b_indx].name);
    noBodyMesh++;
    nextBody:;
  } /* end of for (bod=0; bod<set[setNr].anz_b; bod++) */
  return(noBodyMesh);
 noMesh:;
  return(-1);
}


void meshSet(char *setname, int blockFlag, int lonlyFlag, int projFlag, int meshoptFlag_length, int meshoptFlag_angle )
{
  int setNr, i,j,k, n=0,e=0,p,l,s,b, anz_n, buf, sets;
  double xn, yn, zn;

  int *ptr=NULL, *nodbuf=NULL;

  int ini_anz_e, n1,n2;

#if TEST
  /* file fuer debugging infos */
  handle= fopen("fort.30","w+");
#endif

  nurbsflag=projFlag;
  meshopt_length=meshoptFlag_length;
  meshopt_angle=meshoptFlag_angle;
  oldmeshflag=lonlyFlag;

  writeCFDflag=blockFlag;
  anz_cfdSurfs=0;
  sum_nbuf=0;

  /* clear special sets */
  delSet(specialset->nomesh );

  setNr=getSetNr(setname);
  if (setNr<0)
  {
    printf (" ERROR in meshSet: set:%s does not exist\n", setname);
    return;
  }
  apre->n=0;
  apre->e=0;
  apre->f=0;
  apre->g=0;
  apre->emax=0;
  apre->emin=0;
  apre->b=0;
  apre->c=0;
  apre->l=0;
  apre->nmax=0;
  apre->nmin=0;
  npre=NULL;
  ini_anz_e=anz->e;

  /* start the nodes and elems with the specified offset (asgn) */
  anz->nmax+=anz->noffs;
  anz->emax+=anz->eoffs;

  /* rep() the surface for the interior definitions before the mesh is created */
  /* but only if tr3u or tr3g are requested */
  for (j=0; j<set[setNr].anz_s; j++)
  {
    i=set[setNr].surf[j];
    if((surf[i].etyp==7)&&(surf[i].sh>-1)&&(surf[i].eattr==-1))
    {
      if(shape[surf[i].sh].type==4)
      {
        repNurs(shape[surf[i].sh].p[0]);
        untrimNurs(shape[surf[i].sh].p[0]);
      }
    }
  }
  /* repSurf has to follow */
  for (j=0; j<set[setNr].anz_s; j++)
  {
    i=set[setNr].surf[j];
    if((surf[i].etyp==7)&&(surf[i].sh>-1)&&(surf[i].eattr==-1))
    {
      repSurf(i);
    }
  }

  if ( set[setNr].anz_p>0)
  {
    buf= meshPoints(setNr) ;
    if (buf<0) { errMsg(" Nothing to do! Specify element types with elty.\n"); return; }
  }
  if ( set[setNr].anz_l>0)
  {
    buf=meshLines( setNr);
    if (buf<0) { errMsg(" ERROR: severe problem in meshLines \n"); return; }
    else if (buf>0)  errMsg(" %d lines are not meshed, check set %s \n", buf,specialset->nomesh); 
  }
  if ( set[setNr].anz_s>0)
  {
    buf=meshSurfs( setNr) ;
    if (buf<0) { errMsg(" ERROR: severe problem in meshSurfs \n"); return; }
    else if (buf>0)  errMsg(" %d surfs are not meshed, check set %s \n", buf,specialset->nomesh); 
  }
  if ( set[setNr].anz_b>0)
  {
    buf=meshBodies( setNr) ;
    if (buf<0) { errMsg(" ERROR: severe problem in meshBodies \n"); return; }
    else if (buf>0)  errMsg(" %d bodies are not meshed, check set %s \n", buf,specialset->nomesh); 
  }

  /* --------------- all elements are allocated, allocate final nodes -----------------  */

  /* reset all node references if it is no block mesh */
  if(!blockFlag)
  {
    for (i=0; i<=apre->nmax; i++)
    {
      nbuf[i][0]=0;
    }
  }
  
#if TEST
  /* zum testen koennen npre uebernommen werden, funktioniert nicht bei mehrstufiger Vernetzung */
  printf("zum testen werden  npre uebernommen\n");
  if( anz->nmax )
  {
    errMsg("ERROR: in meshSet, no TEST possible with a predefined mesh (anz->nmax==%d)\n", anz->nmax );
    goto nextSet;
  }
  if( apre->n > 0 )
  {
    for (i=1; i<=apre->nmax; i++)
    {
      nod( anz, &node, 0, i, npre[i].nx, npre[i].ny, npre[i].nz , 0 );
      nbuf[i][0]=1; nbuf[i][1]=i;
    }
  }
#else
  for (i=ini_anz_e; i<anz->e; i++)
  {
    if(e_enqire[e_enqire[i].nr].type==1)
    {
      for (j=0; j<8; j++)
      {
        if ((e_enqire[e_enqire[i].nr].nod[j]>apre->nmax)||(e_enqire[e_enqire[i].nr].nod[j]<0)) 
        {
          printf(" ERROR: en:%d > apre->nmax:%d, e:%d corrupted\n"
          , e_enqire[e_enqire[i].nr].nod[j],apre->nmax,e_enqire[i].nr);
          e_enqire[e_enqire[i].nr].nod[j]= 1; 
        }
        else if ( nbuf[e_enqire[e_enqire[i].nr].nod[j]][0]==0 )  /* node not yet allocated */
        {
          xn=npre[e_enqire[e_enqire[i].nr].nod[j]].nx;
          yn=npre[e_enqire[e_enqire[i].nr].nod[j]].ny;
          zn=npre[e_enqire[e_enqire[i].nr].nod[j]].nz;
          nod( anz, &node, 0, anz->nmax+1, xn, yn, zn, 0 );
          nbuf[e_enqire[e_enqire[i].nr].nod[j]][0]=1;
          nbuf[e_enqire[e_enqire[i].nr].nod[j]][1]= anz->nmax;
          e_enqire[e_enqire[i].nr].nod[j]= anz->nmax;
        }
        else /* node is allocated */
        {
          e_enqire[e_enqire[i].nr].nod[j]= nbuf[e_enqire[e_enqire[i].nr].nod[j]][1];
        }
      }
    }
    if(e_enqire[e_enqire[i].nr].type==4)
    {
      for (j=0; j<20; j++)
      {
        if ((e_enqire[e_enqire[i].nr].nod[j]>apre->nmax)||(e_enqire[e_enqire[i].nr].nod[j]<0)) 
        {
          printf(" ERROR: en:%d > apre->nmax:%d, e:%d corrupted\n", e_enqire[e_enqire[i].nr].nod[j],apre->nmax,e_enqire[i].nr);
          e_enqire[e_enqire[i].nr].nod[j]= 1; 
        }
        else if ( nbuf[e_enqire[e_enqire[i].nr].nod[j]][0]==0 )  /* node not yet allocated */
        {
          xn=npre[e_enqire[e_enqire[i].nr].nod[j]].nx;
          yn=npre[e_enqire[e_enqire[i].nr].nod[j]].ny;
          zn=npre[e_enqire[e_enqire[i].nr].nod[j]].nz;
          nod( anz, &node, 0, anz->nmax+1, xn, yn, zn, 0 ); 
          nbuf[e_enqire[e_enqire[i].nr].nod[j]][0]=1;
          nbuf[e_enqire[e_enqire[i].nr].nod[j]][1]= anz->nmax;
          e_enqire[e_enqire[i].nr].nod[j]= anz->nmax;
        }
        else /* node is allocated */
        {
          e_enqire[e_enqire[i].nr].nod[j]= nbuf[e_enqire[e_enqire[i].nr].nod[j]][1];
        }
      }
    }
    if(e_enqire[e_enqire[i].nr].type==7)
    {
      for (j=0; j<3; j++)
      {
        if ((e_enqire[e_enqire[i].nr].nod[j]>apre->nmax)||(e_enqire[e_enqire[i].nr].nod[j]<0)) 
        {
          printf(" ERROR: en:%d > apre->nmax:%d, e:%d corrupted\n", e_enqire[e_enqire[i].nr].nod[j],apre->nmax,e_enqire[i].nr);
          e_enqire[e_enqire[i].nr].nod[j]= 1; 
        }
        else if (nbuf[ e_enqire[e_enqire[i].nr].nod[j]][0]==0 )  /* node not yet allocated */
        {
          xn=npre[e_enqire[e_enqire[i].nr].nod[j]].nx;
          yn=npre[e_enqire[e_enqire[i].nr].nod[j]].ny;
          zn=npre[e_enqire[e_enqire[i].nr].nod[j]].nz;
          nod( anz, &node, 0, anz->nmax+1, xn, yn, zn, 0 );
          nbuf[e_enqire[e_enqire[i].nr].nod[j]][0]=1;
          nbuf[e_enqire[e_enqire[i].nr].nod[j]][1]= anz->nmax;
          e_enqire[e_enqire[i].nr].nod[j]= anz->nmax;
        }
        else /* node is allocated */
        {
          e_enqire[e_enqire[i].nr].nod[j]= nbuf[e_enqire[e_enqire[i].nr].nod[j]][1];
        }
      }
    }
    if(e_enqire[e_enqire[i].nr].type==8)
    {
      for (j=0; j<6; j++)
      {
        if ((e_enqire[e_enqire[i].nr].nod[j]>apre->nmax)||(e_enqire[e_enqire[i].nr].nod[j]<0)) 
        {
          printf(" ERROR: en:%d > apre->nmax:%d, e:%d corrupted\n", e_enqire[e_enqire[i].nr].nod[j],apre->nmax,e_enqire[i].nr);
          e_enqire[e_enqire[i].nr].nod[j]= 1; 
        }
        else if ( nbuf[e_enqire[e_enqire[i].nr].nod[j]][0]==0 )  /* node not yet allocated */
        {
          xn=npre[e_enqire[e_enqire[i].nr].nod[j]].nx;
          yn=npre[e_enqire[e_enqire[i].nr].nod[j]].ny;
          zn=npre[e_enqire[e_enqire[i].nr].nod[j]].nz;
          nod( anz, &node, 0, anz->nmax+1, xn, yn, zn, 0 );
          nbuf[e_enqire[e_enqire[i].nr].nod[j]][0]=1;
          nbuf[e_enqire[e_enqire[i].nr].nod[j]][1]= anz->nmax;
          e_enqire[e_enqire[i].nr].nod[j]= anz->nmax;
        }
        else /* node is allocated */
        {
          e_enqire[e_enqire[i].nr].nod[j]= nbuf[e_enqire[e_enqire[i].nr].nod[j]][1];
        }
      }
    }
    if(e_enqire[e_enqire[i].nr].type==9)
    {
      for (j=0; j<4; j++)
      {
        if ((e_enqire[e_enqire[i].nr].nod[j]>apre->nmax)||(e_enqire[e_enqire[i].nr].nod[j]<0)) 
        {
          printf(" ERROR: en:%d > apre->nmax:%d, e:%d corrupted\n", e_enqire[e_enqire[i].nr].nod[j],apre->nmax,e_enqire[i].nr);
          e_enqire[e_enqire[i].nr].nod[j]= 1; 
        }
        else if ( nbuf[e_enqire[e_enqire[i].nr].nod[j]][0]==0 )  /* node not yet allocated */
        {
          xn=npre[e_enqire[e_enqire[i].nr].nod[j]].nx;
          yn=npre[e_enqire[e_enqire[i].nr].nod[j]].ny;
          zn=npre[e_enqire[e_enqire[i].nr].nod[j]].nz;
          nod( anz, &node, 0, anz->nmax+1, xn, yn, zn, 0 );
          nbuf[e_enqire[e_enqire[i].nr].nod[j]][0]=1;
          nbuf[e_enqire[e_enqire[i].nr].nod[j]][1]= anz->nmax;
          e_enqire[e_enqire[i].nr].nod[j]= anz->nmax;
        }
        else /* node is allocated */
        {
          e_enqire[e_enqire[i].nr].nod[j]= nbuf[e_enqire[e_enqire[i].nr].nod[j]][1];
        }
      }
    }
    if(e_enqire[e_enqire[i].nr].type==10)
    {
      for (j=0; j<8; j++)
      {
        if ((e_enqire[e_enqire[i].nr].nod[j]>apre->nmax)||(e_enqire[e_enqire[i].nr].nod[j]<0))  
        {
          printf(" ERROR: en:%d > apre->nmax:%d, e:%d corrupted\n", e_enqire[e_enqire[i].nr].nod[j],apre->nmax,e_enqire[i].nr);
          e_enqire[e_enqire[i].nr].nod[j]= 1; 
        }
        else if ( nbuf[e_enqire[e_enqire[i].nr].nod[j]][0]==0 )  /* node not yet allocated */
        {
          xn=npre[e_enqire[e_enqire[i].nr].nod[j]].nx;
          yn=npre[e_enqire[e_enqire[i].nr].nod[j]].ny;
          zn=npre[e_enqire[e_enqire[i].nr].nod[j]].nz;
          nod( anz, &node, 0, anz->nmax+1, xn, yn, zn, 0 );
          nbuf[e_enqire[e_enqire[i].nr].nod[j]][0]=1;
          nbuf[e_enqire[e_enqire[i].nr].nod[j]][1]= anz->nmax;
          e_enqire[e_enqire[i].nr].nod[j]= anz->nmax;
        }
        else /* node is allocated */
        {
          e_enqire[e_enqire[i].nr].nod[j]= nbuf[e_enqire[e_enqire[i].nr].nod[j]][1];
        }
      }
    }
    if(e_enqire[e_enqire[i].nr].type==11)
    {
      for (j=0; j<2; j++)
      {
        if ((e_enqire[e_enqire[i].nr].nod[j]>apre->nmax)||(e_enqire[e_enqire[i].nr].nod[j]<0)) 
        {
          printf(" ERROR: en:%d > apre->nmax:%d, e:%d corrupted\n", e_enqire[e_enqire[i].nr].nod[j],apre->nmax,e_enqire[i].nr);
          e_enqire[e_enqire[i].nr].nod[j]= 1; 
        }
        else if ( nbuf[e_enqire[e_enqire[i].nr].nod[j]][0]==0 )  /* node not yet allocated */
        {
          xn=npre[e_enqire[e_enqire[i].nr].nod[j]].nx;
          yn=npre[e_enqire[e_enqire[i].nr].nod[j]].ny;
          zn=npre[e_enqire[e_enqire[i].nr].nod[j]].nz;
          nod( anz, &node, 0, anz->nmax+1, xn, yn, zn, 0 );
          nbuf[e_enqire[e_enqire[i].nr].nod[j]][0]=1;
          nbuf[e_enqire[e_enqire[i].nr].nod[j]][1]= anz->nmax;
          e_enqire[e_enqire[i].nr].nod[j]= anz->nmax;
        }
        else /* node is allocated */
        {
          e_enqire[e_enqire[i].nr].nod[j]= nbuf[e_enqire[e_enqire[i].nr].nod[j]][1];
        }
      }
    }
    if(e_enqire[e_enqire[i].nr].type==12)
    {
      for (j=0; j<3; j++)
      {
        if ((e_enqire[e_enqire[i].nr].nod[j]>apre->nmax)||(e_enqire[e_enqire[i].nr].nod[j]<0)) 
        {
          printf(" ERROR: en[%d]:%d > apre->nmax:%d, e:%d corrupted\n", i, e_enqire[e_enqire[i].nr].nod[j],apre->nmax,e_enqire[i].nr);
          e_enqire[e_enqire[i].nr].nod[j]= 1; 
        }
        else if ( nbuf[e_enqire[e_enqire[i].nr].nod[j]][0]==0 )  /* node not yet allocated */
        {
          xn=npre[e_enqire[e_enqire[i].nr].nod[j]].nx;
          yn=npre[e_enqire[e_enqire[i].nr].nod[j]].ny;
          zn=npre[e_enqire[e_enqire[i].nr].nod[j]].nz;
          nod( anz, &node, 0, anz->nmax+1, xn, yn, zn, 0 );
          nbuf[e_enqire[e_enqire[i].nr].nod[j]][0]=1;
          nbuf[e_enqire[e_enqire[i].nr].nod[j]][1]= anz->nmax;
          e_enqire[e_enqire[i].nr].nod[j]= anz->nmax;
        }
        else /* node is allocated */
        {
          e_enqire[e_enqire[i].nr].nod[j]= nbuf[e_enqire[e_enqire[i].nr].nod[j]][1];
        }
      }
    }
  }
#endif

  /* replace n_pre by node in all cfd-blocks */
  if(printFlag) printf(" update the node numbers of the cfd-blocks\n");
  if ( apre->b>0)
  {
    anz->b=apre->b;
    anz->c=apre->c;
    for (i=0; i<anz->b; i++)
    {
      for (n=0; n<(nBlock[i].i*nBlock[i].j*nBlock[i].k); n++ )
      {
        nBlock[i].nod[n]=nbuf[nBlock[i].nod[n]][1];
      }
    }
  }

  /* replace n_pre by node in all entities */
  if(printFlag) printf(" update the node numbers of the geometric entities\n");
  if ( set[setNr].anz_p>0)
  {
    for (i=0; i<set[setNr].anz_p; i++)
    {
      anz_n=0;
      p=set[setNr].pnt[i];
      for (n=0; n<point[p].nn; n++ )
      {
#if TEST
        if ((point[p].nod[n]>apre->nmax)||(point[p].nod[n]<0))
	{
          errMsg("ERROR: point[%d].nod[%d]=%d not valid\n",p,n,point[p].nod[n]);
        }
#endif
        j=0; while(j<nbuf[point[p].nod[n]][0])
        {
          if ((nodbuf = (int *)realloc((int *)nodbuf, (anz_n+1)*sizeof(int)) ) == NULL )
          { printf("\n\nERROR: realloc failure in meshSet\n\n"); return; }
          nodbuf[anz_n]=nbuf[point[p].nod[n]][++j]; anz_n++;
        }
      }
      point[p].nn=anz_n;
      ptr=point[p].nod;
      point[p].nod=nodbuf;
      nodbuf=ptr;
    }
  }

  if ( set[setNr].anz_l>0)
  {
    for (i=0; i<set[setNr].anz_l; i++)
    {
      anz_n=0;
      l=set[setNr].line[i];
      for (n=0; n<line[l].nn; n++ )
      {
#if TEST
        if ((line[l].nod[n]>apre->nmax)||(line[l].nod[n]<0))
	{
          errMsg("ERROR: line[%d].nod[%d]=%d not valid\n",l,n,line[l].nod[n]);
        }
#endif
        j=0; while(j<nbuf[line[l].nod[n]][0])
        {
          if ((nodbuf = (int *)realloc((int *)nodbuf, (anz_n+1)*sizeof(int)) ) == NULL )
          { printf("\n\nERROR: realloc failure in meshSet\n\n"); return; }
          nodbuf[anz_n]=nbuf[line[l].nod[n]][++j]; anz_n++;
        }
      }
      line[l].nn=anz_n;
      ptr=line[l].nod;
      line[l].nod=nodbuf;
      nodbuf=ptr;
    }
  }

  if ( set[setNr].anz_s>0)
  {
    for (i=0; i<set[setNr].anz_s; i++)
    {
      anz_n=0;
      s=set[setNr].surf[i];
      for (n=0; n<surf[s].nn; n++  )
      {
#if TEST
        if ((surf[s].nod[n]>apre->nmax)||(surf[s].nod[n]<0)) 
	{
          errMsg("ERROR: surf[%d].nod[%d]=%d not valid\n",s,n,surf[s].nod[n]);  
        }
#endif
        j=0; while(j<nbuf[surf[s].nod[n]][0])
        {
          if ((nodbuf = (int *)realloc((int *)nodbuf, (anz_n+1)*sizeof(int)) ) == NULL )
          { printf("\n\nERROR: realloc failure in meshSet\n\n"); return; }
          nodbuf[anz_n]=nbuf[surf[s].nod[n]][++j]; anz_n++;
        }
      }
      surf[s].nn=anz_n;
      ptr=surf[s].nod;
      surf[s].nod=nodbuf;
      nodbuf=ptr;
    }
  }

  if ( set[setNr].anz_b>0)
  {
    for (i=0; i<set[setNr].anz_b; i++)
    {
      anz_n=0;
      b=set[setNr].body[i];
      for (n=0; n<body[b].nn; n++ )
      {
#if TEST
        if ((body[b].nod[n]>apre->nmax)||(body[b].nod[n]<1))
	{
          errMsg("ERROR: body[%d].nod[%d]=%d not valid\n",b,n,body[b].nod[n]);  
        }
#endif
        j=0; while(j<nbuf[body[b].nod[n]][0])
        {
          if ((nodbuf = (int *)realloc((int *)nodbuf, (anz_n+1)*sizeof(int)) ) == NULL )
          { printf("\n\nERROR: realloc failure in meshSet\n\n"); return; }
          nodbuf[anz_n]=nbuf[body[b].nod[n]][++j]; anz_n++;
        }
      }
      body[b].nn=anz_n;
      ptr=body[b].nod;
      body[b].nod=nodbuf;
      nodbuf=ptr;
    }
  }

  /* delete the temporary entities which were created to substitute 3- and 5-sided surfs */
  /* warning, s is now redefined */
  if(printFlag) printf(" delete the temporary entities\n");
  s=getSetNr(specialset->zap); 
#if TEST
  fclose(handle);
#else
  if(s>-1) 
  {
    for(i=0; i<set[s].anz_b; i++)
    {
      if(printFlag) printf (" delete body:%s \n",  body[set[s].body[i]].name );
      setr( 0, "b",set[s].body[i] );
      body[set[s].body[i]].name = (char *)NULL ;
      body[set[s].body[i]].ns=0;
      free(body[set[s].body[i]].o);
      body[set[s].body[i]].o= NULL;
      free(body[set[s].body[i]].s);
      body[set[s].body[i]].s= NULL;
      body[set[s].body[i]].nn=-1;
      free(body[set[s].body[i]].nod);
      body[set[s].body[i]].nod= NULL;
      body[set[s].body[i]].ne=-1;
      free(body[set[s].body[i]].elem);
      body[set[s].body[i]].elem= NULL;      
      body[set[s].body[i]].etyp= 0;
    }
    for(i=0; i<set[s].anz_s; i++)
    {
      if(printFlag) printf (" delete surf:%s \n",  surf[set[s].surf[i]].name );
      setr( 0, "s",set[s].surf[i] );
      surf[set[s].surf[i]].name = (char *)NULL ;
      surf[set[s].surf[i]].nl= 0;
      free(surf[set[s].surf[i]].typ);
      surf[set[s].surf[i]].typ= NULL;
      free(surf[set[s].surf[i]].o);
      surf[set[s].surf[i]].o= NULL;
      free(surf[set[s].surf[i]].l);
      surf[set[s].surf[i]].l= NULL;
      surf[set[s].surf[i]].nn= -1;
      free(surf[set[s].surf[i]].nod);
      surf[set[s].surf[i]].nod= NULL;
      surf[set[s].surf[i]].ne= -1;
      free(surf[set[s].surf[i]].elem);
      surf[set[s].surf[i]].elem= NULL;
      surf[set[s].surf[i]].etyp= 0;
    }
    for(i=0; i<set[s].anz_l; i++)
    {
      if(printFlag) printf (" delete line:%s \n",  line[set[s].line[i]].name );
      /* setr will also remove node-numbers of n_pre from set 'all' */
      /* this node-numbers might be used by predefined nodes also */
      /* therefore a 'comp all do' must follow to compensate this */
      setr( 0, "l",set[s].line[i] );
      line[set[s].line[i]].name = (char *)NULL ;
      line[set[s].line[i]].div = 0;
      if (line[set[s].line[i]].typ=='s')
      {
        /* delete the set */
        delSet(set[line[set[s].line[i]].trk].name);
      }
      line[set[s].line[i]].typ=' ';
      line[set[s].line[i]].etyp=0;
      line[set[s].line[i]].p1=-1;
      line[set[s].line[i]].p2=-1;
      line[set[s].line[i]].trk=-1;
      line[set[s].line[i]].nip= 0;
      free(line[set[s].line[i]].ip);
      line[set[s].line[i]].ip= NULL;
      line[set[s].line[i]].nn= -1;
      free(line[set[s].line[i]].nod);
      line[set[s].line[i]].nod = NULL;
      line[set[s].line[i]].ne= -1;
      free(line[set[s].line[i]].elem);
      line[set[s].line[i]].elem = NULL;
    }
    for(i=0; i<set[s].anz_c; i++)
    {
      if(printFlag) printf (" delete lcmb:%s \n",  lcmb[set[s].lcmb[i]].name );
      setr( 0, "c",set[s].lcmb[i] );
      lcmb[set[s].lcmb[i]].name = (char *)NULL;
      lcmb[set[s].lcmb[i]].nl=0;
      free(lcmb[set[s].lcmb[i]].o);
      lcmb[set[s].lcmb[i]].o= NULL;
      free(lcmb[set[s].lcmb[i]].l);
      lcmb[set[s].lcmb[i]].l= NULL;
      lcmb[set[s].lcmb[i]].p1=-1;
      lcmb[set[s].lcmb[i]].p2=-1;
    }
    for(i=0; i<set[s].anz_p; i++)
    {
      if(printFlag) printf (" delete pnt:%s \n",  point[set[s].pnt[i]].name );
      setr( 0, "p",set[s].pnt[i] );
      point[set[s].pnt[i]].name = (char *)NULL ; 
      free(point[set[s].pnt[i]].nod);
      point[set[s].pnt[i]].nod=NULL; 
      point[set[s].pnt[i]].nn=-1; 
    }
    /* delete the set itself */
    delSet(specialset->zap);

    /* the following commands includes the lost nodes in set all */
    s=pre_seta( specialset->zap, "i", 0 );
    setall=getSetNr("all");
    if(setall>=0)
    {
      if((set[s].elem=(int *)realloc((int *)set[s].elem,(set[s].anz_e+set[setall].anz_e+1)*sizeof(int)))==NULL)
        printf(" ERROR: malloc failed in set[%d]:%s\n\n", 0, set[s].name);
      for(i=0; i<set[setall].anz_e; i++)
      {
        set[s].elem[set[s].anz_e]= set[setall].elem[i]; set[s].anz_e++;
      }
      qsort( set[s].elem, set[s].anz_e, sizeof(int), (void *)compareInt );
    }
    /* circle through all elements and add all nodes */
    for (i=0; i<set[s].anz_e; i++)
    {
      if (e_enqire[set[s].elem[i]].type == 1) n = 8;       /* HEXA8 */
      else if (e_enqire[set[s].elem[i]].type == 2) n = 6;  /* PENTA6 */
      else if (e_enqire[set[s].elem[i]].type == 3) n = 4;  /* TET4 */
      else if (e_enqire[set[s].elem[i]].type == 4) n = 20; /* HEXA20 */
      else if (e_enqire[set[s].elem[i]].type == 5) n = 15; /* PENTA15 */
      else if (e_enqire[set[s].elem[i]].type == 6) n = 10; /* TET10 */
      else if (e_enqire[set[s].elem[i]].type == 7) n = 3;  /* TRI3  */
      else if (e_enqire[set[s].elem[i]].type == 8) n = 6;  /* TRI6  */
      else if (e_enqire[set[s].elem[i]].type == 9) n = 4;  /* QUAD4 */
      else if (e_enqire[set[s].elem[i]].type == 10) n = 8; /* QUAD8 */
      else if (e_enqire[set[s].elem[i]].type == 11) n = 2; /* BEAM2 */
      else if (e_enqire[set[s].elem[i]].type == 12) n = 3; /* BEAM3 */
      else n=0;
      if((set[s].node=(int *)realloc((int *)set[s].node,(set[s].anz_n+n+1)*sizeof(int)))==NULL)
      printf(" ERROR: malloc failed in set[%d]:%s\n\n", 0, set[s].name);
      for (j=0; j<n; j++)
      {
        set[s].node[set[s].anz_n]= e_enqire[set[s].elem[i]].nod[j]; set[s].anz_n++;
      }
    }
    qsort( set[s].node, set[s].anz_n, sizeof(int), (void *)compareInt );

    for(i=0; i<set[s].anz_n; i++) if(!node[set[s].node[i]].pflag)
    {
      seta(setall,"n",set[s].node[i]);
    }

    delSet(specialset->zap);
  }
#endif

  /* add the nodes of entities to the sets */
  for (sets=0; sets<anz->sets; sets++)
  {
    if ( sets==setNr||( set[sets].type==1)||(set[sets].name==(char *)NULL)) goto nextSet;
    n=set[sets].anz_n;
    e=set[sets].anz_e;

    if ( set[sets].anz_p>0)
    {
      for (i=0; i<set[sets].anz_p; i++)
      {
        p=set[sets].pnt[i];
        if((point[p].name != (char *)NULL)&&(point[p].nn>0))
        {
          set[sets].anz_n+=point[p].nn;
          if((set[sets].node=(int *)realloc((int *)set[sets].node, (set[sets].anz_n+1)*sizeof(int)))==NULL)
          { printf("\nERROR: realloc failure in meshSet\n\n"); return; }
          for (k=0; k<point[p].nn; k++)
          {
            set[sets].node[n++]=point[p].nod[k];
          }
        }
      }
    }
    if ( set[sets].anz_l>0)
    {
      for (i=0; i<set[sets].anz_l; i++)
      {
        l=set[sets].line[i];
        if(line[l].name != (char *)NULL)
        {
          if(line[l].nn>0) set[sets].anz_n+=line[l].nn;
          if(line[l].ne>0) set[sets].anz_e+=line[l].ne;
          if((set[sets].node=(int *)realloc((int *)set[sets].node, (set[sets].anz_n+1)*sizeof(int)))==NULL)
          { printf("\nERROR: realloc failure in meshSet\n\n"); return; }
          if((set[sets].elem=(int *)realloc((int *)set[sets].elem, (set[sets].anz_e+1)*sizeof(int)))==NULL)
          { printf("\nERROR: realloc failure in meshSet\n\n"); return; }
          for (k=0; k<line[l].nn; k++)
          {
            set[sets].node[n++]=line[l].nod[k];
          }
          for (k=0; k<line[l].ne; k++)
          {
            set[sets].elem[e++]=line[l].elem[k];
          }
        }
      }
    }
    if ( set[sets].anz_s>0)
    {
      for (i=0; i<set[sets].anz_s; i++)
      {
        s=set[sets].surf[i];
        if(surf[s].name != (char *)NULL)
        {
          if(surf[s].nn>0) set[sets].anz_n+=surf[s].nn;
          if(surf[s].ne>0) set[sets].anz_e+=surf[s].ne;
          if((set[sets].node=(int *)realloc((int *)set[sets].node, (set[sets].anz_n+1)*sizeof(int)))==NULL)
          { printf("\nERROR: realloc failure in meshSet\n\n"); return; }
          if((set[sets].elem=(int *)realloc((int *)set[sets].elem, (set[sets].anz_e+1)*sizeof(int)))==NULL)
          { printf("\nERROR: realloc failure in meshSet\n\n"); return; }
          for (k=0; k<surf[s].nn; k++)
          {
            set[sets].node[n++]=surf[s].nod[k];
          }
          for (k=0; k<surf[s].ne; k++)
          {
            set[sets].elem[e++]=surf[s].elem[k];
          }
        }
      }
    }
    if ( set[sets].anz_b>0)
    {
      for (i=0; i<set[sets].anz_b; i++)
      {
        b=set[sets].body[i];
        if(body[b].name != (char *)NULL)
        {
          if(body[b].nn>0) set[sets].anz_n+=body[b].nn;
          if(body[b].ne>0) set[sets].anz_e+=body[b].ne;
          if((set[sets].node=(int *)realloc((int *)set[sets].node, (set[sets].anz_n+1)*sizeof(int)))==NULL)
          { printf("\nERROR: realloc failure in meshSet\n\n"); return; }
          if((set[sets].elem=(int *)realloc((int *)set[sets].elem, (set[sets].anz_e+1)*sizeof(int)))==NULL)
          { printf("\nERROR: realloc failure in meshSet\n\n"); return; }
          for (k=0; k<body[b].nn; k++)
          {
            set[sets].node[n++]=body[b].nod[k];
          }
          for (k=0; k<body[b].ne; k++)
          {
            set[sets].elem[e++]=body[b].elem[k];
          }
        }
      }
    }
    qsort( set[sets].node, set[sets].anz_n, sizeof(int), (void *)compareInt );
    qsort( set[sets].elem, set[sets].anz_e, sizeof(int), (void *)compareInt );

    /* erase multiple entities */
    if(set[sets].anz_n)
    {
      n=0;
      for(j=1; j<set[sets].anz_n; j++)
      {
        if(set[sets].node[n]!=set[sets].node[j]) set[sets].node[++n]=set[sets].node[j];
      }
      set[sets].anz_n=n+1;
    }
    if(set[sets].anz_e)
    {
      n=0;
      for(j=1; j<set[sets].anz_e; j++)
      {
        if(set[sets].elem[n]!=set[sets].elem[j]) set[sets].elem[++n]=set[sets].elem[j];
      }
      set[sets].anz_e=n+1;
    }
  nextSet:;
  }

  for(i=0; i<apre->nmax; i++) free(nbuf[i]);
  free(nbuf);
  nbuf=NULL;

  free(npre);
  npre=NULL;
  apre->n=0;
  apre->nmax=0;
  apre->nmin=0;


  /* for drawing purposes it is nessesary to add additional nodes */
  anz->orignmax = anz->nmax;
  anz->orign = anz->n;

  for(i=0; i<anz->e; i++)
  {   
   if(e_enqire[e_enqire[i].nr].type==4)
   {
      for (n=0; n<3; n++)  /* create new nodes in center of areas */
      {
        nod( anz, &node, 0, anz->nmax+1, 0., 0., 0., 0 ); 
        node[anz->nmax].pflag=1;

        node[anz->nmax].nx = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[0+n]].nx+node[e_enqire[e_enqire[i].nr].nod[1+n]].nx    +
          node[e_enqire[e_enqire[i].nr].nod[5+n]].nx+node[e_enqire[e_enqire[i].nr].nod[4+n]].nx )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[8+n]].nx+node[e_enqire[e_enqire[i].nr].nod[13+n]].nx   +
          node[e_enqire[e_enqire[i].nr].nod[16+n]].nx+node[e_enqire[e_enqire[i].nr].nod[12+n]].nx) ;

        node[anz->nmax].ny = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[0+n]].ny+node[e_enqire[e_enqire[i].nr].nod[1+n]].ny    +
          node[e_enqire[e_enqire[i].nr].nod[5+n]].ny+node[e_enqire[e_enqire[i].nr].nod[4+n]].ny )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[8+n]].ny+node[e_enqire[e_enqire[i].nr].nod[13+n]].ny   +
          node[e_enqire[e_enqire[i].nr].nod[16+n]].ny+node[e_enqire[e_enqire[i].nr].nod[12+n]].ny) ;

        node[anz->nmax].nz = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[0+n]].nz+node[e_enqire[e_enqire[i].nr].nod[1+n]].nz    +
          node[e_enqire[e_enqire[i].nr].nod[5+n]].nz+node[e_enqire[e_enqire[i].nr].nod[4+n]].nz )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[8+n]].nz+node[e_enqire[e_enqire[i].nr].nod[13+n]].nz   +
          node[e_enqire[e_enqire[i].nr].nod[16+n]].nz+node[e_enqire[e_enqire[i].nr].nod[12+n]].nz) ;

        e_enqire[e_enqire[i].nr].nod[n+20]=anz->nmax;
      }

      /* create  new node in center of area4 */

        nod( anz, &node, 0, anz->nmax+1, 0., 0., 0., 0 ); 
        node[anz->nmax].pflag=1;

        node[anz->nmax].nx = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[3]].nx+node[e_enqire[e_enqire[i].nr].nod[0]].nx    +
          node[e_enqire[e_enqire[i].nr].nod[4]].nx+node[e_enqire[e_enqire[i].nr].nod[7]].nx )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[11]].nx+node[e_enqire[e_enqire[i].nr].nod[12]].nx   +
          node[e_enqire[e_enqire[i].nr].nod[19]].nx+node[e_enqire[e_enqire[i].nr].nod[15]].nx) ;

        node[anz->nmax].ny = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[3]].ny+node[e_enqire[e_enqire[i].nr].nod[0]].ny    +
          node[e_enqire[e_enqire[i].nr].nod[4]].ny+node[e_enqire[e_enqire[i].nr].nod[7]].ny )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[11]].ny+node[e_enqire[e_enqire[i].nr].nod[12]].ny   +
          node[e_enqire[e_enqire[i].nr].nod[19]].ny+node[e_enqire[e_enqire[i].nr].nod[15]].ny) ;

        node[anz->nmax].nz = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[3]].nz+node[e_enqire[e_enqire[i].nr].nod[0]].nz    +
          node[e_enqire[e_enqire[i].nr].nod[4]].nz+node[e_enqire[e_enqire[i].nr].nod[7]].nz )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[11]].nz+node[e_enqire[e_enqire[i].nr].nod[12]].nz   +
          node[e_enqire[e_enqire[i].nr].nod[19]].nz+node[e_enqire[e_enqire[i].nr].nod[15]].nz) ;

        e_enqire[e_enqire[i].nr].nod[23]=anz->nmax;

      for (n=0; n<2; n++)  /* create last 2 new nodes in center of areas */
      {
        nod( anz, &node, 0, anz->nmax+1, 0., 0., 0., 0 ); 
        node[anz->nmax].pflag=1;

        n1=n*4;
        n2=n*8;
        node[anz->nmax].nx = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[0+n1]].nx+node[e_enqire[e_enqire[i].nr].nod[1+n1]].nx    +
          node[e_enqire[e_enqire[i].nr].nod[2+n1]].nx+node[e_enqire[e_enqire[i].nr].nod[3+n1]].nx )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[8+n2]].nx+node[e_enqire[e_enqire[i].nr].nod[9+n2]].nx   +
          node[e_enqire[e_enqire[i].nr].nod[10+n2]].nx+node[e_enqire[e_enqire[i].nr].nod[11+n2]].nx) ;

        node[anz->nmax].ny = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[0+n1]].ny+node[e_enqire[e_enqire[i].nr].nod[1+n1]].ny    +
          node[e_enqire[e_enqire[i].nr].nod[2+n1]].ny+node[e_enqire[e_enqire[i].nr].nod[3+n1]].ny )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[8+n2]].ny+node[e_enqire[e_enqire[i].nr].nod[9+n2]].ny   +
          node[e_enqire[e_enqire[i].nr].nod[10+n2]].ny+node[e_enqire[e_enqire[i].nr].nod[11+n2]].ny) ;

        node[anz->nmax].nz = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[0+n1]].nz+node[e_enqire[e_enqire[i].nr].nod[1+n1]].nz    +
          node[e_enqire[e_enqire[i].nr].nod[2+n1]].nz+node[e_enqire[e_enqire[i].nr].nod[3+n1]].nz )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[8+n2]].nz+node[e_enqire[e_enqire[i].nr].nod[9+n2]].nz   +
          node[e_enqire[e_enqire[i].nr].nod[10+n2]].nz+node[e_enqire[e_enqire[i].nr].nod[11+n2]].nz) ;

        e_enqire[e_enqire[i].nr].nod[n+24]=anz->nmax;
    }
   }
   if(e_enqire[e_enqire[i].nr].type==5)
   {
      if ( (node = (Nodes *)realloc( (Nodes *)node, (anz->nmax+6) * sizeof(Nodes))) == NULL )
        printf("\n\n ERROR: realloc failed node\n\n") ;
      for (n=0; n<2; n++)  
      {
        anz->nmax++;
        node[anz->n].nr = anz->nmax;
        node[anz->nmax].pflag=1;

        node[anz->nmax].nx = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[0+n]].nx+node[e_enqire[e_enqire[i].nr].nod[1+n]].nx    +
          node[e_enqire[e_enqire[i].nr].nod[4+n]].nx+node[e_enqire[e_enqire[i].nr].nod[3+n]].nx )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[6+n]].nx+node[e_enqire[e_enqire[i].nr].nod[10+n]].nx   +
          node[e_enqire[e_enqire[i].nr].nod[12+n]].nx+node[e_enqire[e_enqire[i].nr].nod[ 9+n]].nx) ;

        node[anz->nmax].ny = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[0+n]].ny+node[e_enqire[e_enqire[i].nr].nod[1+n]].ny    +
          node[e_enqire[e_enqire[i].nr].nod[4+n]].ny+node[e_enqire[e_enqire[i].nr].nod[3+n]].ny )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[6+n]].ny+node[e_enqire[e_enqire[i].nr].nod[10+n]].ny   +
          node[e_enqire[e_enqire[i].nr].nod[12+n]].ny+node[e_enqire[e_enqire[i].nr].nod[ 9+n]].ny) ;

        node[anz->nmax].nz = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[0+n]].nz+node[e_enqire[e_enqire[i].nr].nod[1+n]].nz    +
          node[e_enqire[e_enqire[i].nr].nod[4+n]].nz+node[e_enqire[e_enqire[i].nr].nod[3+n]].nz )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[6+n]].nz+node[e_enqire[e_enqire[i].nr].nod[10+n]].nz   +
          node[e_enqire[e_enqire[i].nr].nod[12+n]].nz+node[e_enqire[e_enqire[i].nr].nod[ 9+n]].nz) ;

        e_enqire[ e_enqire[i].nr ].nod[n+15]=node[anz->n].nr;
        anz->n++;
      }
        anz->nmax++;
        node[anz->n].nr = anz->nmax;
        node[anz->nmax].pflag=1;

        node[anz->nmax].nx = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[2]].nx+node[e_enqire[e_enqire[i].nr].nod[0]].nx    +
          node[e_enqire[e_enqire[i].nr].nod[3]].nx+node[e_enqire[e_enqire[i].nr].nod[5]].nx )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[ 8]].nx+node[e_enqire[e_enqire[i].nr].nod[ 9]].nx   +
          node[e_enqire[e_enqire[i].nr].nod[14]].nx+node[e_enqire[e_enqire[i].nr].nod[11]].nx) ;

        node[anz->nmax].ny = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[2]].ny+node[e_enqire[e_enqire[i].nr].nod[0]].ny    +
          node[e_enqire[e_enqire[i].nr].nod[3]].ny+node[e_enqire[e_enqire[i].nr].nod[5]].ny )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[ 8]].ny+node[e_enqire[e_enqire[i].nr].nod[ 9]].ny   +
          node[e_enqire[e_enqire[i].nr].nod[14]].ny+node[e_enqire[e_enqire[i].nr].nod[11]].ny) ;

        node[anz->nmax].nz = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[2]].nz+node[e_enqire[e_enqire[i].nr].nod[0]].nz    +
          node[e_enqire[e_enqire[i].nr].nod[3]].nz+node[e_enqire[e_enqire[i].nr].nod[5]].nz )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[ 8]].nz+node[e_enqire[e_enqire[i].nr].nod[ 9]].nz   +
          node[e_enqire[e_enqire[i].nr].nod[14]].nz+node[e_enqire[e_enqire[i].nr].nod[11]].nz) ;

        e_enqire[ e_enqire[i].nr ].nod[17]=node[anz->n].nr;
        anz->n++;

        anz->nmax++;
        node[anz->n].nr = anz->nmax;
        node[anz->nmax].pflag=1;

        node[anz->nmax].nx = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[0]].nx+node[e_enqire[e_enqire[i].nr].nod[2]].nx    +
          node[e_enqire[e_enqire[i].nr].nod[1]].nx+node[e_enqire[e_enqire[i].nr].nod[0]].nx )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[ 8]].nx+node[e_enqire[e_enqire[i].nr].nod[ 7]].nx   +
          node[e_enqire[e_enqire[i].nr].nod[ 6]].nx+node[e_enqire[e_enqire[i].nr].nod[ 0]].nx) ;

        node[anz->nmax].ny = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[0]].ny+node[e_enqire[e_enqire[i].nr].nod[2]].ny    +
          node[e_enqire[e_enqire[i].nr].nod[1]].ny+node[e_enqire[e_enqire[i].nr].nod[0]].ny )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[ 8]].ny+node[e_enqire[e_enqire[i].nr].nod[ 7]].ny   +
          node[e_enqire[e_enqire[i].nr].nod[ 6]].ny+node[e_enqire[e_enqire[i].nr].nod[ 0]].ny) ;

        node[anz->nmax].nz = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[0]].nz+node[e_enqire[e_enqire[i].nr].nod[2]].nz    +
          node[e_enqire[e_enqire[i].nr].nod[1]].nz+node[e_enqire[e_enqire[i].nr].nod[0]].nz )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[ 8]].nz+node[e_enqire[e_enqire[i].nr].nod[ 7]].nz   +
          node[e_enqire[e_enqire[i].nr].nod[ 6]].nz+node[e_enqire[e_enqire[i].nr].nod[ 0]].nz) ;

        e_enqire[ e_enqire[i].nr ].nod[18]=node[anz->n].nr;
        anz->n++;

        anz->nmax++;
        node[anz->n].nr = anz->nmax;
        node[anz->nmax].pflag=1;

        node[anz->nmax].nx = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[3]].nx+node[e_enqire[e_enqire[i].nr].nod[4]].nx    +
          node[e_enqire[e_enqire[i].nr].nod[5]].nx+node[e_enqire[e_enqire[i].nr].nod[3]].nx )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[12]].nx+node[e_enqire[e_enqire[i].nr].nod[13]].nx   +
          node[e_enqire[e_enqire[i].nr].nod[14]].nx+node[e_enqire[e_enqire[i].nr].nod[ 3]].nx) ;

        node[anz->nmax].ny = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[3]].ny+node[e_enqire[e_enqire[i].nr].nod[4]].ny    +
          node[e_enqire[e_enqire[i].nr].nod[5]].ny+node[e_enqire[e_enqire[i].nr].nod[3]].ny )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[12]].ny+node[e_enqire[e_enqire[i].nr].nod[13]].ny   +
          node[e_enqire[e_enqire[i].nr].nod[14]].ny+node[e_enqire[e_enqire[i].nr].nod[ 3]].ny) ;

        node[anz->nmax].nz = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[3]].nz+node[e_enqire[e_enqire[i].nr].nod[4]].nz    +
          node[e_enqire[e_enqire[i].nr].nod[5]].nz+node[e_enqire[e_enqire[i].nr].nod[3]].nz )  + 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[12]].nz+node[e_enqire[e_enqire[i].nr].nod[13]].nz   +
          node[e_enqire[e_enqire[i].nr].nod[14]].nz+node[e_enqire[e_enqire[i].nr].nod[ 3]].nz) ;

        e_enqire[ e_enqire[i].nr ].nod[19]=node[anz->n].nr;
        anz->n++;
   }
   if(e_enqire[e_enqire[i].nr].type==10)
   {
      nod( anz, &node, 0, anz->nmax+1, 0., 0., 0., 0 ); 
      node[anz->nmax].pflag=1;

        node[anz->nmax].nx = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[0]].nx+node[e_enqire[e_enqire[i].nr].nod[1]].nx  +
          node[e_enqire[e_enqire[i].nr].nod[3]].nx+node[e_enqire[e_enqire[i].nr].nod[2]].nx )+ 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[4]].nx+node[e_enqire[e_enqire[i].nr].nod[6]].nx  +
          node[e_enqire[e_enqire[i].nr].nod[7]].nx+node[e_enqire[e_enqire[i].nr].nod[5]].nx) ;

        node[anz->nmax].ny = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[0]].ny+node[e_enqire[e_enqire[i].nr].nod[1]].ny  +
          node[e_enqire[e_enqire[i].nr].nod[3]].ny+node[e_enqire[e_enqire[i].nr].nod[2]].ny )+ 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[4]].ny+node[e_enqire[e_enqire[i].nr].nod[6]].ny  +
          node[e_enqire[e_enqire[i].nr].nod[7]].ny+node[e_enqire[e_enqire[i].nr].nod[5]].ny) ;

        node[anz->nmax].nz = -1./4.* (
          node[e_enqire[e_enqire[i].nr].nod[0]].nz+node[e_enqire[e_enqire[i].nr].nod[1]].nz  +
          node[e_enqire[e_enqire[i].nr].nod[3]].nz+node[e_enqire[e_enqire[i].nr].nod[2]].nz )+ 1./2.*(
          node[e_enqire[e_enqire[i].nr].nod[4]].nz+node[e_enqire[e_enqire[i].nr].nod[6]].nz  +
          node[e_enqire[e_enqire[i].nr].nod[7]].nz+node[e_enqire[e_enqire[i].nr].nod[5]].nz) ;
        e_enqire[e_enqire[i].nr].nod[8]=anz->nmax;
   }
  }

  elemChecker( anz->emax+1, node, e_enqire);
  if(printFlag) printf(" end of meshSet\n");
}
