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

/* Bugs:
*/

#define TEST  0

/* temporary conversion from old to new bias definition */
extern int OLD_BIAS_DEF;
int        old_bias_def;

#include <cgx.h>
#include <sys/utsname.h>

extern struct utsname  cursys[1];

extern int     ddiv;
extern double     dbias;

extern Scale     scale[1];
extern Summen    anz[1];
extern Nodes     *node;
extern Elements  *e_enqire;
extern Datasets *lcase;

extern Alias     *alias;
extern Sets      *set;
extern Shapes    *shape;
extern Points    *point;
extern Lines     *line;
extern Lcmb      *lcmb;
extern Gsur      *surf;
extern Gbod      *body;
extern Nurbl     *nurbl;
extern Nurbs     *nurbs;
extern SumGeo    anzGeo[1];
extern SumAsci   sumAsci[1];


extern char  printFlag;                     /* printf on/off */
extern char  delPntFlag;                    /* 1: deleted points exists */
extern char  delShapeFlag;                  /* 1: deleted shapes exists */
extern char  delLineFlag;                   /* 1: deleted lines exists */
extern char  delLcmbFlag;                   /* 1: deleted lcmbs exists */
extern char  delSurfFlag;                   /* 1: deleted surfs exists */
extern char  delBodyFlag;                   /* 1: deleted bodys exists */
extern char  delNursFlag;

extern SpecialSet specialset[1];
extern GLint   gl_max_eval_order;                         /* max order of NURBS */




/*------------------------------------------------------------------*/
/* define node                                                      */
/*------------------------------------------------------------------*/

void delNod( int anzn, int *index ) 
{
  int j,k;
  int *buf;  /* must be, else error because *index could be from a set (which is manipul.) */

  if( (buf=(int *)malloc((anzn+1)*sizeof(int) ) )==NULL) 
  { printf(" ERROR: malloc failure\n"); return; }
  for (j=0; j<anzn; j++) buf[j]=index[j];

  for (j=0; j<anzn; j++)
  {
    if(printFlag) printf (" delete node:%d\n",buf[j] );
    node[buf[j]].pflag=-1;
    for (k=0; k<anz->sets; k++)
    {
      if( set[k].name != (char *)NULL ) setr( k, "n", buf[j]);
    }
  }
  free(buf);

  /* search the last valid node */
  while(node[anz->nmax].pflag==-1) { anz->nmax--; anz->n--; }
}


/* warning, before use: delete all midface-nodes for he20 etc. (see pre_nod()) */
/* or you might use a certain node-nr twice! */
int nod( Summen *anz, Nodes **nptr, int setFlag, int nodnr, double x, double y, double z, int scalFlag)
{
  static int nr, i, lc;
  static Nodes *node;
  node=*nptr;
  if(setFlag) { nr=getNodNr(anz, node, nodnr); }
  else nr=-1;
  if (nr==-1)        /* new node */
  {
    if(nodnr>anz->nmax)
    {
      if ((node = (Nodes *)realloc( (Nodes *)node, (nodnr+1)*sizeof(Nodes)) ) == NULL )
      { errMsg("ERROR: realloc failure in nod, node:%d not installed\n", nodnr); return(-1); }
      for(i=anz->nmax+1; i<=nodnr; i++) node[i].indx=-1;
      anz->nmax=nodnr;
    }
    if ( nodnr < anz->nmin ) anz->nmin=nodnr;
    *nptr =  node;
    nr=anz->n;
    node[nr].nr=nodnr;
    node[node[nr].nr].indx=nr;
    anz->n++;
    if (setFlag)
    {
      for (i=0; i<anz->sets; i++)
      {
        if ( set[i].flag=='o') seta( i, "n", nodnr );
      }
    }

    /* extend the lc by the new node */
    for (lc=0; lc<anz->l; lc++)
    {
     if (lcase[lc].loaded)
     {
      for(i=0; i<lcase[lc].ncomps; i++)
      {
        if ( (lcase[lc].dat[i] = (float *)realloc(lcase[lc].dat[i], (anz->nmax+1) * sizeof(float))) == NULL )
          printf("\n\n ERROR: realloc failure nod\n\n" );
        lcase[lc].dat[i][nodnr]=0.;
      }
     }
    }
    if(printFlag) printf (" new Node %d nodnr %d anz->nmax %d \n", nr, nodnr,anz->nmax);
  }
  else if(nr<-1)    /* replace a deleted node */
  {
    nr=-(nr+10);
    if(printFlag) printf (" Node %d %d %d, use a deleted node\n", nr, nodnr,anz->nmax);
  }
  else
  {
    if(printFlag) printf (" Node %d %d %d in use: Koordinates will be changed\n", nr, nodnr,node[nr].nr);
  }
  node[node[nr].nr].pflag=0;

  if(scalFlag)
  {
    node[nodnr].nx=(x-scale->x)/scale->w;
    node[nodnr].ny=(y-scale->y)/scale->w;
    node[nodnr].nz=(z-scale->z)/scale->w;
  }
  else
  {
    node[nodnr].nx=x;
    node[nodnr].ny=y;
    node[nodnr].nz=z;
  }
  
  return(nr);
}

void pre_nod( char *record)
{
  int  length, nr;
  double x=0.,y=0.,z=0.;

  length=sscanf( record, "%d %lf %lf %lf\n", &nr, &x, &y, &z);
  if(printFlag) printf("node:%d x:%lf y:%lf z:%lf \n", nr, x, y, z);

  /* free the additional midside-nodes for higher order elements */
  anz->n= anz->orign;
  anz->nmax=anz->orignmax;

  nr=nod( anz, &node, 1, nr, x, y, z, 1);
  if( nr <0)
    printf("ERROR: node could not be created\n");

  /* new number of original nodes */
  anz->orignmax = anz->nmax;
  anz->orign = anz->n;

  /* new midnodes */
  adjustDrawNodes(1);
}


/*------------------------------------------------------------------*/
/* define elements                                                 */
/*------------------------------------------------------------------*/

/* delElem() if introduced must provide that e_enquire[].nr is defined without holes */
void delElem( int anze, int *index ) 
{
  int j,k,e,n,anz_e,anz_emax,ipuf;
  int *buf;  /* must be, else error because *index could be from a set (which is manipul.) */
  Elements  *elem=NULL;

  if( (buf=(int *)malloc((anze+1)*sizeof(int) ) )==NULL) 
  { printf(" ERROR: malloc failure\n"); return; }
  for (j=0; j<anze; j++) buf[j]=index[j];

  for (j=0; j<anze; j++)
  {
    if(printFlag) printf (" delete element:%d\n",buf[j] );
    if(e_enqire[buf[j]].type>0)
    {
      for (k=0; k<anzGeo->l; k++)
      {  
        for (n=0; n<line[k].ne; n++)
        {
          if(surf[k].elem[n]==buf[j]) line[k].elem[n]=0;
        }
        e=0;
        for(n=0; n<line[k].ne; n++) if(line[k].elem[n]>0) line[k].elem[e++]=line[k].elem[n];
        line[k].ne=e; 
      }
      for (k=0; k<anzGeo->s; k++)
      {  
        for (n=0; n<surf[k].ne; n++)
        {
          if(surf[k].elem[n]==buf[j]) surf[k].elem[n]=0;
        }
        e=0;
        for(n=0; n<surf[k].ne; n++) if(surf[k].elem[n]>0) surf[k].elem[e++]=surf[k].elem[n];
        surf[k].ne=e; 
      }
      for (k=0; k<anzGeo->b; k++)
      {  
        for (n=0; n<body[k].ne; n++)
        {
          if(body[k].elem[n]==buf[j]) body[k].elem[n]=0;
        }
        e=0;
        for(n=0; n<body[k].ne; n++) if(body[k].elem[n]>0) body[k].elem[e++]=body[k].elem[n];
        body[k].ne=e; 
      }
    }
    for (k=0; k<anz->sets; k++)
    {
      if( set[k].name != (char *)NULL )
      {
        setr( k, "e", buf[j]);
        free(set[k].face);
        set[k].face=(int *)NULL;
        set[k].anz_f = 0;
      }
    }
    e_enqire[buf[j]].type=0;
  }
  free(buf);

  /* create a new element data structure */
  anz_e=anz->e;
  anz_emax=anz->emax;
  for(j=0; j<100; j++) anz->etype[j]=0;
  anz->emax=0;  anz->emin=MAX_INTEGER;
  anz->e=0;
  if((elem=(Elements *)realloc((Elements *)elem,(anz_emax+1)*sizeof(Elements)))==NULL)
  {
    printf("\n\n ERROR: realloc failed, elements:%d\n\n", anz_emax);
    return;
  }

  for(e=0; e<anz_e; e++)
  {
    j=e_enqire[e].nr;
    if(e_enqire[j].type)
    {
      elem[anz->e].nr   = j;
      elem[anz->e].type = e_enqire[j].type;
      elem[anz->e].group= e_enqire[j].group;
      elem[anz->e].mat  = e_enqire[j].mat;
      elem[anz->e].attr  = e_enqire[j].attr;
      ipuf=0;
      if (elem[anz->e].nr >  anz->emax)  anz->emax=elem[anz->e].nr;
      if (elem[anz->e].nr <  anz->emin)  anz->emin=elem[anz->e].nr;
      if (elem[anz->e].type == 1)      ipuf = 8;   /* HEXA8  */
      else if (elem[anz->e].type == 2) ipuf = 6;   /* PE6   */
      else if (elem[anz->e].type == 3) ipuf = 4;   /* TET4   */
      else if (elem[anz->e].type == 4) ipuf = 20;  /* HEXA20 */
      else if (elem[anz->e].type == 5) ipuf = 15;  /* PE15  */
      else if (elem[anz->e].type == 6) ipuf = 10;  /* TET10  */
      else if (elem[anz->e].type == 7) ipuf = 3;   /* TRI3   */
      else if (elem[anz->e].type == 8) ipuf = 6;   /* TRI6   */
      else if (elem[anz->e].type == 9) ipuf = 4;   /* QUAD4  */
      else if (elem[anz->e].type == 10) ipuf = 10; /* QUAD8  */
      else if (elem[anz->e].type == 11) ipuf = 2;  /* BEAM2   */
      else if (elem[anz->e].type == 12) ipuf = 3;  /* BEAM3   */

      anz->etype[elem[anz->e].type]++;
      for(n=0; n<ipuf; n++) elem[anz->e].nod[n]= e_enqire[j].nod[n];
      anz->e++;
    }
  }
  if(!anz->e) anz->emin=0;

  /* initialize the new element data structure */
  iniElements(anz, elem, anz_e);
  free(elem); elem=NULL;
  updateDispLists();
}



int elem_define( int elnr, int type, int *node, int setFlag, int eattr )
{
  int nr, ipuf=0,ipuf2=0, n, i,nf=0;

  if (elnr<=anz->emax) nr=e_enqire[elnr].type;
  else nr=0;
  if (nr==0)     /* new elem */
  {
    if (elnr>anz->emax)     /* new elem */
    {
      if((e_enqire=(Elements *)realloc((Elements *)e_enqire, (elnr+1) * sizeof(Elements))) == NULL )
        printf("\n\n ERROR: malloc failed in main() e_enqire\n\n") ;
      for(i=anz->emax+1; i<elnr; i++)  e_enqire[i].type = 0;
      anz->emax=elnr;
    }
    if ( elnr < anz->emin ) anz->emin=elnr;
    e_enqire[anz->e].nr   = elnr;
    e_enqire[elnr].side = NULL;
    anz->e++;
  }
  else
  {
    if(printFlag) printf (" Elem:%d in use: Def will be changed\n", elnr);

    /* free space for the normal-vectors */
    if(e_enqire[ elnr ].side!=NULL)
    {
      if (e_enqire[ elnr ].type == 1)       nf=6;  /* HEXA8 */
      else if (e_enqire[ elnr ].type == 2)  nf=6;  /* PENTA6 */
      else if (e_enqire[ elnr ].type == 3)  nf=4;  /* TET4 */
      else if (e_enqire[ elnr ].type == 4)  nf=48; /* HEXA20 */
      else if (e_enqire[ elnr ].type == 5)  nf=48; /* PENTA15 */
      else if (e_enqire[ elnr ].type == 6)  nf=16; /* TET10 */
      else if (e_enqire[ elnr ].type == 7)  nf=1;  /* TRI3  */
      else if (e_enqire[ elnr ].type == 8)  nf=4; /* TRI6  */
      else if (e_enqire[ elnr ].type == 9)  nf=2; /* QUAD4 */
      else if (e_enqire[ elnr ].type == 10) nf=8; /* QUAD8 */
      else if (e_enqire[ elnr ].type == 11) nf=1; /* BEAM */
      else if (e_enqire[ elnr ].type == 12) nf=1; /* BEAM3 */
      for(i=0; i<nf; i++) free(e_enqire[ elnr ].side[i]);
      free(e_enqire[ elnr ].side);
      e_enqire[ elnr ].side = NULL;
    }
  }

  e_enqire[ elnr ].group   = 0;
  e_enqire[ elnr ].mat     = 1;
  e_enqire[ elnr ].attr    = eattr;
  e_enqire[ elnr ].type    = type ;
  if (setFlag) for (i=0; i<anz->sets; i++) if ( set[i].flag=='o') seta( i, "e", elnr );

  if (e_enqire[ elnr ].type == 1) ipuf = 8;  /* HEXA8 */
  else if (e_enqire[ elnr ].type == 2) ipuf = 6;  /* PENTA6 */
  else if (e_enqire[ elnr ].type == 3) ipuf = 4;  /* TET4 */
  else if (e_enqire[ elnr ].type == 4) ipuf = 20; /* HEX20 */
  else if (e_enqire[ elnr ].type == 5) ipuf = 15; /* PENTA15 */
  else if (e_enqire[ elnr ].type == 6) ipuf = 10; /* TET10 */
  else if (e_enqire[ elnr ].type == 7) ipuf = 3;  /* TRI3  */
  else if (e_enqire[ elnr ].type == 8) ipuf = 6;  /* TRI6  */
  else if (e_enqire[ elnr ].type == 9) ipuf = 4;  /* QUAD4 */
  else if (e_enqire[ elnr ].type == 10) ipuf = 8; /* QUAD8 */
  else if (e_enqire[ elnr ].type == 11) ipuf = 2; /* BEAM */
  else if (e_enqire[ elnr ].type == 12) ipuf = 3; /* BEAM3 */
  if (ipuf!=0) for (n=0; n<ipuf; n++)
  {
    e_enqire[ elnr ].nod[n]=node[n];
  }
  /* set the midside nodenr to 0 */
  if (e_enqire[ elnr ].type == 4) ipuf2 = 26; /* HEX20 */
  else if (e_enqire[ elnr ].type == 5) ipuf2 = 20; /* PENTA15 */
  else if (e_enqire[ elnr ].type == 10) ipuf2 = 9; /* QUAD8 */
  if (ipuf2!=0) for (n=ipuf; n<ipuf2; n++)
  {
    e_enqire[ elnr ].nod[n]=0;
  }

  /* space for the normal-vectors */
  if (e_enqire[ elnr ].type == 1)       nf=6;  /* HEXA8 */
  else if (e_enqire[ elnr ].type == 2)  nf=6;  /* PENTA6 */
  else if (e_enqire[ elnr ].type == 3)  nf=4;  /* TET4 */
  else if (e_enqire[ elnr ].type == 4)  nf=48; /* HEXA20 */
  else if (e_enqire[ elnr ].type == 5)  nf=48; /* PENTA15 */
  else if (e_enqire[ elnr ].type == 6)  nf=16; /* TET10 */
  else if (e_enqire[ elnr ].type == 7)  nf=1;  /* TRI3  */
  else if (e_enqire[ elnr ].type == 8)  nf=4; /* TRI6  */
  else if (e_enqire[ elnr ].type == 9)  nf=2; /* QUAD4 */
  else if (e_enqire[ elnr ].type == 10) nf=8; /* QUAD8 */
  else if (e_enqire[ elnr ].type == 11) nf=1; /* BEAM */
  else if (e_enqire[ elnr ].type == 12) nf=1; /* BEAM3 */

  if((e_enqire[ elnr ].side=(double **)malloc((nf)*sizeof(double *)))==NULL)
    printf("\n\n ERROR: malloc failed\n\n" );
  for(i=0; i<nf; i++)
  {
    if((e_enqire[ elnr ].side[i]=(double *)malloc((3)*sizeof(double)))==NULL)
      printf("\n\n ERROR: malloc failed\n\n" );
  }

#if TEST
  printf("e:%d ", elnr);
  if (ipuf!=0) for (n=0; n<ipuf; n++)
  {
    printf("n:%d ",node[n]);
  }
  printf("\n ");
#endif

  return(nr);
}



void pre_elem( char *record)
{
  int i, type;
  int  length, nr[27], args=0, ptr=0;
  char dat[MAX_LINE_LENGTH];

  /* lese eingabe mit variabler laenge  */
  for (i=0; i<22; i++)
  {
    length= sword( &record[ptr], dat );
    if(atoi(dat)==0) break;
    nr[i]=atoi( dat);
    ptr+=length+1;
    args++;
  }

  if(printFlag) printf("elem: ");
  for (i=0; i<args; i++)
  {
    if(printFlag) printf("%d, ", nr[i]);
  }
  if(printFlag) printf("%s \n", dat);

  for(i=0; i<strlen(dat); i++) dat[i]=toupper(dat[i]);
  if (compare( dat, "HE8", 3)==3 ) type=1;
  else if (compare( dat, "HE20", 3)==3 ) type=4;
  else if (compare( dat, "TR3", 3)==3 ) type=7;
  else if (compare( dat, "TR6", 3)==3 ) type=8;
  else if (compare( dat, "QU4", 3)==3 ) type=9;
  else if (compare( dat, "QU8", 3)==3 ) type=10;
  else if (compare( dat, "BE2", 3)==3 ) type=11;
  else if (compare( dat, "BE3", 3)==3 ) type=12;
  else
  {
    printf(" element type:%s not known\n", dat);
    return;
  }
  i=elem_define( nr[0], type, &nr[1], 1, 0 );
  if( i <0)
    errMsg("WARNING: element could not be created, might be changed\n");
}


/*------------------------------------------------------------------*/
/* define alias                                                     */
/*------------------------------------------------------------------*/

int hashAlias( SumAsci *sumAsci, char *name, int nr)
{
  int i=0,j=0;
  int sum=0;

  while(name[i]!='\0') { sum+=name[i]*(++j); i++;}
  
  /* check if sum is higher as the allocated value */
  if(sum>sumAsci->max_suma)
  {
    if ((sumAsci->anza=(int *)realloc( (int *)sumAsci->anza, (sum+1)*sizeof(int)) ) == NULL )
    { printf("\n\nERROR: realloc failure in hashAlias(), Alias:%s not included\n\n", name); return(-1); }
    if ((sumAsci->aindx=(int **)realloc( (int **)sumAsci->aindx, (sum+1)*sizeof(int *)) ) == NULL )
    { printf("\n\nERROR: realloc failure in hashAlias(), Alias:%s not included\n\n", name); return(-1); }
    for(i=sumAsci->max_suma+1; i<=sum; i++) { sumAsci->anza[i]=0; sumAsci->aindx[i]=NULL; }
    sumAsci->max_suma=sum;
  }

  /* alloc of a new entry in the hash table */
  if ((sumAsci->aindx[sum] 
  =(int *)realloc( (int *)sumAsci->aindx[sum], (sumAsci->anza[sum]+1)*sizeof(int)) ) == NULL )
  { printf("\n\nERROR: realloc failure in hashAlias(), alias:%s not included\n\n", name); return(-1); }

  sumAsci->aindx[sum][sumAsci->anza[sum]] = nr;
  sumAsci->anza[sum]++;
  return(sum);
}


void operateAlias( char *name, char *type )
{
  int length, i, alnam, sum;

  if( name[0] == '%')
  {
    /* existing alias found, search real name  */
    length= strlen( name );
    if(printFlag) printf (" rename %s ", name );
    for (i=0; i<length; i++) name[i] = name[i+1];
    if(printFlag) printf (" to aliasName %s  \n", name );
    i=getAliasNr( name );
    if( i >-1)
    {
      if(printFlag) printf(" existing alias found:%s, real name:%s\n", name, alias[i].entitieName);
      strcpy( name, alias[i].entitieName );
      return;
    }
    if(printFlag) printf(" found no real name for alias:%s \n", name);
  }
  else if( name[0] == '!')
  {
    /* Alias found, generate new entity-name  */
    length= strlen( name );
    if (length==1) 
    {
      if ( getNewName( name, type ) == -1 )
      { printf(" ERROR: operateAlias: Type %s not known\n", type); }
      return;
    }
    if(printFlag) printf (" rename %s ", name );
    for (i=0; i<length; i++) name[i] = name[i+1];
    if(printFlag) printf (" to aliasName %s  \n", name );

    /* is the name in use? if yes, overwrite him  */
    alnam=getAliasNr(name);
    if (alnam==-1)
    {
      if((alias=(Alias *)realloc((Alias *)alias,(anzGeo->alias+1)*sizeof(Alias)))==NULL)
      { errMsg(" ERROR: realloc failure in operateAlias\n");
      return; }
      alnam=anzGeo->alias;                   /* new alias */
      sum=hashAlias( sumAsci, name, alnam );
      anzGeo->alias++;
    }

    i=strlen(name);
    if((alias[alnam].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return; }
    strcpy( alias[alnam].name, name );
    if ( getNewName( name, type ) == -1 )
      { printf(" ERROR: operateAlias: Type %s not known\n", type); }
    i=strlen(name);
    if((alias[alnam].entitieName= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return; }
    strcpy( alias[alnam].entitieName, name );
    if(printFlag) printf("  real name:%s\n", alias[alnam].entitieName);
  }
}

/*------------------------------------------------------------------*/
/* define point                                                    */
/*------------------------------------------------------------------*/

void delPnt( int anzp, int *number )
{
  int j, k, *nrbuffer;

  delPntFlag=1;
  if ((nrbuffer = (int *)malloc((anzp+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in delPnt\n\n"); return; }

  /* nessesary to store the numbers in a independent area  */
  for (j=0; j<anzp; j++)
    nrbuffer[j]=number[j];

  for (j=0; j<anzp; j++) if( point[nrbuffer[j]].name != (char *)NULL )
  {
    /* remove the points from all sets */ 
    for (k=0; k<anz->sets; k++)
    {
     if( set[k].name != (char *)NULL )
     {
      if(set[k].type==0)
        setr( k, "p", nrbuffer[j]);
      else
        seqr( k, "p", nrbuffer[j] );
     }
    }
    if(printFlag) printf (" delete pnt:%s \n",  point[nrbuffer[j]].name );
    free(point[nrbuffer[j]].name);
    point[nrbuffer[j]].name = (char *)NULL ; 
    point[nrbuffer[j]].nn= -1;
    free(point[nrbuffer[j]].nod);
    point[nrbuffer[j]].nod = NULL;
  }
  free(nrbuffer);
}

int hashPoint( SumAsci *sumAsci, char *name, int nr)
{
  int i=0,j=0, n;
  int sum=0;

  while(name[i]!='\0') { sum+=name[i]*(++j); i++;}

  /* check if sum is higher as the allocated value */
  /* else look for a free entry */
  if(sum>sumAsci->max_sump)
  {
    if ((sumAsci->anzp=(int *)realloc( (int *)sumAsci->anzp, (sum+1)*sizeof(int)) ) == NULL )
    { printf("\n\nERROR: realloc failure in hashPoint(), Point:%s not included\n\n", name); return(-1); }
    if ((sumAsci->pindx=(int **)realloc( (int **)sumAsci->pindx, (sum+1)*sizeof(int *)) ) == NULL )
    { printf("\n\nERROR: realloc failure in hashPoint(), Point:%s not included\n\n", name); return(-1); }
    for(i=sumAsci->max_sump+1; i<=sum; i++) { sumAsci->anzp[i]=0; sumAsci->pindx[i]=NULL; }
    sumAsci->max_sump=sum;
  }
  else
  {
    if (delPntFlag)
    for (i=0; i<sumAsci->anzp[sum]; i++) 
    {
      n=sumAsci->pindx[sum][i];
      if( point[n].name == (char *)NULL ) 
      {
        /* already existing space to fill */
        sumAsci->pindx[sum][i]=nr;
        return(sum);
      }
    }
  }

  /* alloc of a new entry in the hash table */
  if ((sumAsci->pindx[sum] 
  =(int *)realloc( (int *)sumAsci->pindx[sum], (sumAsci->anzp[sum]+1)*sizeof(int)) ) == NULL )
  { printf("\n\nERROR: realloc failure in hashPoint(), pnt:%s not included\n\n", name); return(-1); }

  sumAsci->pindx[sum][sumAsci->anzp[sum]] = nr;
  sumAsci->anzp[sum]++;
  return(sum);
}


int pnt( char *name, double x, double y, double z, int scalFlag )
{
  /* static char name[MAX_LINE_LENGTH]; */
  int nr, i;
  int sum=0;
  nr=getPntNr(name);

  if (nr==-1)        /* new point */
  {
    if ((point = (Points *)realloc( (Points *)point, (anzGeo->p+1)*sizeof(Points)) ) == NULL )
    { printf("\n\nERROR: realloc failure in pnt, pnt:%s not installed\n\n", name); return(-1); }
    nr=anzGeo->p;
    i=strlen(name);
    if((point[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(point[nr].name, name);
    sum=hashPoint( sumAsci, name, nr );
    anzGeo->p++;
    point[nr].nod = NULL;
  }
  else if (nr<-1)    /* replace a deleted point */
  {
    nr=-(nr+10);
    i=strlen(name);
    if((point[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(point[nr].name, name);
    sum=hashPoint( sumAsci, name, nr );
  }
  else
  {
    if(printFlag) printf (" Point in use: Coordinates will be changed\n");
    free(point[nr].nod);
    point[nr].nod = NULL;
  }
  point[nr].nn= -1;

  if(scalFlag)
  {
    point[nr].px=(x-scale->x)/scale->w;
    point[nr].py=(y-scale->y)/scale->w;
    point[nr].pz=(z-scale->z)/scale->w;
  }
  else
  {
    point[nr].px=x;
    point[nr].py=y;
    point[nr].pz=z;
  }

  /* print the values of the hash table */
  /*
  else printf(" asciSum:%d asci-indx:%d sumP in hash-table:%d name:%s index:%d\n"
  , sum, sumAsci->anzp[sum]-1, sumAsci->anzp[sum]
  , point[sumAsci->pindx[sum][sumAsci->anzp[sum]-1]].name
  , sumAsci->pindx[sum][sumAsci->anzp[sum]-1] );  
  */
  for (i=0; i<anz->sets; i++)
  {
    if(( set[i].name!=(char *)NULL)&&( set[i].flag=='o')) seta( i, "p", nr );
  } 
  return(nr);
}


#define   RATIO_FACTOR  10000  /* makes integer from double < 1. */

int pre_pnt( char *record, int addFlag)
{
  int   i, length, setNr, nr, pnr=-1, noName=0;
  char  name[MAX_LINE_LENGTH];
  char  xbuf[MAX_LINE_LENGTH], ybuf[MAX_LINE_LENGTH];
  double          fbuf, p0[3], p1[3], p01[3], p01t[3], bias;
  double          x=0.,y=0.,z=0.,t=1.;

  xbuf[0]=ybuf[0]=0;

  if (!addFlag) length=sscanf (record,"%s%s%s%lg%lg", name, xbuf, ybuf, &z, &t );
  else { name[0]='!'; length=sscanf (record,"%s%s%s%lg%lg", &name[1], xbuf, ybuf, &z, &t ); } 

  /* check if no name is specified */
  if((strlen(name)==1)&&(name[0]=='!')) noName=1;
  operateAlias( name, "p" );

  if (length==2)
  {
     /* make points from nodes, names are automatically generated */
     setNr=getSetNr(xbuf);

     if (setNr<0)
     {
       x=atof(xbuf);
       if(printFlag) printf (" pnt=%s x=%lf y=%lf z=%lf\n",  name, x, y, z);
       if( (pnr=pnt( name, x, y, z, 1 )) <0) printf("ERROR: point could not be created\n");
     }
     else
     {
       for (i=0; i<set[setNr].anz_n; i++)
       {
         if(noName) getNewName( name, "p" );
         else noName=1;
         if(printFlag) printf (" pnt=%s x=%lf y=%lf z=%lf\n",  name, x, y, z);
         if( (pnr=pnt( name
	       , node[set[setNr].node[i]].nx
	       , node[set[setNr].node[i]].ny
		       , node[set[setNr].node[i]].nz, 0 )) <0) printf("ERROR: point could not be created\n");
         pre_seta( set[setNr].name, "p", name );
       }
     }
  }
  else if(xbuf[0]>'9') /* xbuf is not a number, could be a line or a point */
  {
    if(ybuf[0]>'9') /* xbuf is not a number, must be a point */
    {
      /* create pnt from ratio, names are automatically generated */
      /* bestimme die Koordinaten der Linienendpunkte */
      nr= getPntNr( xbuf );
      if(nr<0) { printf("ERROR: pnt:%s not defined\n", xbuf); return(-1); }
      p0[0] = point[nr].px;
      p0[1] = point[nr].py;
      p0[2] = point[nr].pz;
      nr= getPntNr( ybuf );
      if(nr<0) { printf("ERROR: pnt:%s not defined\n", xbuf); return(-1); }
      p1[0] = point[nr].px;
      p1[1] = point[nr].py;
      p1[2] = point[nr].pz;
      v_result( p0, p1, p01 );
      for(i=1; i<=t; i++)
      {
        fbuf=z*i;
        v_scal( &fbuf, p01, p1 );
        v_add( p0, p1, p01t );
        if(noName) getNewName( name, "p" );
        else noName=1;
        if(printFlag) printf (" pnt %s %lf %lf %lf\n", name, p01t[0]*scale->w+scale->x, p01t[1]*scale->w+scale->y, p01t[2]*scale->w+scale->z);
        if( (pnr=pnt( name, p01t[0], p01t[1], p01t[2], 0 )) <0) printf("ERROR: point could not be created\n");
      }
    }
    else /* point from line */
    {
      nr= getLineNr( xbuf );
      if(nr<0) { printf("ERROR: line:%s not defined\n", xbuf); return(-1); }

      x=atof(ybuf);
      if(!z) z=1;
      if(z*x>1) z=1/x;
      bias=line[nr].bias;
      line[nr].bias=1.;
      for(i=1; i<=z; i++)
      {
        if (line[nr].typ=='a')
        {
          arcNodes( nr, RATIO_FACTOR*x*i, RATIO_FACTOR, p01t );
        }
        else if (line[nr].typ=='s')
        {
          splineNodes( nr, RATIO_FACTOR*x*i, RATIO_FACTOR, p01t );
        }
        else if (line[nr].typ=='n')
        {
           nurlNodes( nr, RATIO_FACTOR*x*i, RATIO_FACTOR, p01t );
        }
        else
        {
          straightNodes( nr, RATIO_FACTOR*x*i, RATIO_FACTOR, p01t );
        }
        if(noName) getNewName( name, "p" );
        else noName=1;
        if(printFlag) printf (" pnt %s %lf %lf %lf\n", name, p01t[0]*scale->w+scale->x, p01t[1]*scale->w+scale->y, p01t[2]*scale->w+scale->z);
        if( (pnr=pnt( name, p01t[0], p01t[1], p01t[2], 0 )) <0) printf("ERROR: point could not be created\n");
      }
      line[nr].bias=bias;
    }
  }
  else
  {
    x=atof(xbuf);
    y=atof(ybuf);
    if(printFlag) printf (" pnt %s %lf %lf %lf\n", name, x, y, z);
    if( (pnr=pnt( name, x, y, z, 1 )) <0) printf("ERROR: point could not be created\n");
  }
  return(pnr);
}


/*------------------------------------------------------------------*/
/* define shape                                                     */
/*------------------------------------------------------------------*/

void delShape( int anzs, int *number )
{
  int j, k, *nrbuffer;

  delShapeFlag=1;
  
  // printf ("sum:%d num:%d shape:%s\n", anzs, number[0], shape[number[0]].name );
  
  if ((nrbuffer = (int *)malloc((anzs+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in delShape\n\n"); return; }

  /* nessesary to store the numbers in a independent area  */
  for (j=0; j<anzs; j++)
    nrbuffer[j]=number[j];

  for (j=0; j<anzs; j++) if( shape[nrbuffer[j]].name != (char *)NULL )
  {
    for (k=0; k<anz->sets; k++)
    {
      if(set[k].type==0)
       if( set[k].name != (char *)NULL )
        setr( k, "sh", nrbuffer[j]);
    }
    if(printFlag) printf (" delete shape:%s \n",  shape[nrbuffer[j]].name );
    free(shape[nrbuffer[j]].name);
    shape[nrbuffer[j]].name = (char *)NULL ;
    shape[nrbuffer[j]].npgn= 0;
    free(shape[nrbuffer[j]].pgn);
    shape[nrbuffer[j]].pgn= NULL;
  }
  free(nrbuffer);
}

int hashShape( SumAsci *sumAsci, char *name, int nr)
{
  int i=0,j=0, n;
  int sum=0;

  while(name[i]!='\0') { sum+=name[i]*(++j); i++; }

  /* check if sum is higher as the allocated value */
  /* else look for a free entry */
  if(sum>sumAsci->max_sumsh)
  {
    if ((sumAsci->anzsh=(int *)realloc( (int *)sumAsci->anzsh, (sum+1)*sizeof(int)) ) == NULL )
    { printf("\n\nERROR: realloc failure in hashShape(), Shape:%s not included\n\n", name); return(-1); }
    if ((sumAsci->shindx=(int **)realloc( (int **)sumAsci->shindx, (sum+1)*sizeof(int *)) ) == NULL )
    { printf("\n\nERROR: realloc failure in hashShape(), Shape:%s not included\n\n", name); return(-1); }
    for(i=sumAsci->max_sumsh+1; i<=sum; i++) { sumAsci->anzsh[i]=0; sumAsci->shindx[i]=NULL; }
    sumAsci->max_sumsh=sum;
  }
  else
  {
    if (delShapeFlag)
    for (i=0; i<sumAsci->anzsh[sum]; i++) 
    {
      n=sumAsci->shindx[sum][i];
      if( shape[n].name == (char *)NULL )
      {
        /* already existing space to fill */
        sumAsci->shindx[sum][i]=nr;
        return(sum);
      }
    }
  }

  /* alloc of a new entry in the hash table */
  if ((sumAsci->shindx[sum]=(int *)realloc( (int *)sumAsci->shindx[sum], (sumAsci->anzsh[sum]+1)*sizeof(int)) ) == NULL )
  { printf("\n\nERROR: realloc failure in hashShape(), shape:%s not included\n\n", name); return(-1); }

  sumAsci->shindx[sum][sumAsci->anzsh[sum]] = nr;
  sumAsci->anzsh[sum]++;
  return(sum);
}


int shape_i( char *name, int type, int ip1, int ip2, int ip3 )
{
  static int nr, i;
  int sum=0;

  nr=getShapeNr(name);
  if (nr==-1)        /* new */
  {
    if ((shape = (Shapes *)realloc( (Shapes *)shape, (anzGeo->sh+1)*sizeof(Shapes)) ) == NULL )
    { printf("\n\nERROR: realloc failure in line, line:%s not installed\n\n", name); return(-1); }
    nr=anzGeo->sh;
    anzGeo->sh++;
    i=strlen(name);
    if((shape[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(shape[nr].name, name);
    sum=hashShape( sumAsci, name, nr );
    shape[nr].npgn= 0;
    shape[nr].pgn= NULL;
  }
  else if (nr<-1)    /* replace a deleted line */
  {
    nr=-(nr+10);
    i=strlen(name);
    if((shape[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(shape[nr].name, name);
    sum=hashShape( sumAsci, name, nr );
  }
  else
  {
    if(printFlag) printf (" shape %s in use: Definition will be changed\n", name);
    shape[nr].npgn= 0;
    free(shape[nr].pgn);
    shape[nr].pgn= NULL;
  }
  shape[nr].type=type;     
  shape[nr].p[0]=ip1;
  shape[nr].p[1]=ip2;
  shape[nr].p[2]=ip3;

  for (i=0; i<anz->sets; i++)
  {
    if(( set[i].name != (char *)NULL)&&( set[i].flag=='o')) seta( i, "sh", nr );
  }

  return(nr);
}


void pre_shape( char *record, int addFlag)
{
  int  i, length;
  static char name[MAX_LINE_LENGTH], type[MAX_LINE_LENGTH], datum[3][MAX_LINE_LENGTH];
  int typi, p1,p2,p3;

  if (!addFlag) length = sscanf( record, "%s%s%s%s%s", name, type, datum[0], datum[1], datum[2] );
  else 
  {
    name[0]='!';
    datum[0][0]=datum[1][0]=datum[2][0]='%';
    length = sscanf( record, "%s%s%s%s%s", &name[1], type, &datum[0][1], &datum[1][1], &datum[2][1] );
  }

  operateAlias( name, "sh" );
  operateAlias( datum[0], "p" );
  operateAlias( datum[1], "p" );
  operateAlias( datum[2], "p" );


  if(printFlag) printf(" shpe %s %s %s %s %s \n", name, type, datum[0], datum[1], datum[2]);

  for(i=0; i<strlen(type); i++) type[i]=toupper(type[i]);
  if (compare(type, "PLN",3)==3) typi=0;
  else return;

  /* check if points exists */
  p1=getPntNr(datum[0]);
  if (p1==-1)        /* new point */
  {
    printf ("ERROR: point %s of Shape %s not defined\n", datum[0], name);
    return;
  }
  p2=getPntNr(datum[1]);
  if (p2==-1)        /* new point */
  {
    printf ("ERROR: point %s of Shape %s not defined\n", datum[1], name);
    return;
  }
  p3=getPntNr(datum[2]);
  if (p3==-1)        /* new point */
  {
    printf ("ERROR: point %s of Shape %s not defined\n", datum[2], name);
    return;
  }
  if( shape_i( name, typi, p1, p2, p3) <0)
    printf("ERROR: shape could not be created\n");
}


/*------------------------------------------------------------------*/
/* define line                                                      */
/*------------------------------------------------------------------*/

void delLine( int anzl, int *number )
{
  int j, k, *nrbuffer;

  delLineFlag=1;
  /*
  printf ("sum:%d num:%d lin:%s\n", anzl, number[0], line[number[0]].name );
  */
  if ((nrbuffer = (int *)malloc((anzl+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in delSurf\n\n"); return; }

  /* nessesary to store the numbers in a independent area  */
  for (j=0; j<anzl; j++)
    nrbuffer[j]=number[j];

  for (j=0; j<anzl; j++) if( line[nrbuffer[j]].name != (char *)NULL )
  {
    for (k=0; k<anz->sets; k++)
    {
      if(set[k].type==0)
       if( set[k].name != (char *)NULL )
        setr( k, "l", nrbuffer[j]);
    }
    if(printFlag) printf (" delete line:%s \n",  line[nrbuffer[j]].name );
    free(line[nrbuffer[j]].name);
    line[nrbuffer[j]].name = (char *)NULL ;
    line[nrbuffer[j]].div = 0;
    if (line[nrbuffer[j]].typ=='s')
    {
      /* delete the set */
      delSet(set[line[nrbuffer[j]].trk].name);
    }
    line[nrbuffer[j]].typ=' ';
    line[nrbuffer[j]].etyp=0;
    line[nrbuffer[j]].p1=-1;
    line[nrbuffer[j]].p2=-1;
    line[nrbuffer[j]].trk=-1;
    line[nrbuffer[j]].nip= -1;
    free(line[nrbuffer[j]].ip);
    line[nrbuffer[j]].ip= NULL;
    line[nrbuffer[j]].nn= -1;
    free(line[nrbuffer[j]].nod);
    line[nrbuffer[j]].nod = NULL;
    line[nrbuffer[j]].ne= -1;
    free(line[nrbuffer[j]].elem);
    line[nrbuffer[j]].elem = NULL;
  }
  free(nrbuffer);
}

int hashLine( SumAsci *sumAsci, char *name, int nr)
{
  int i=0,j=0, n;
  int sum=0;

  while(name[i]!='\0') { sum+=name[i]*(++j); i++; }

  /* check if sum is higher as the allocated value */
  /* else look for a free entry */
  if(sum>sumAsci->max_suml)
  {
    if ((sumAsci->anzl=(int *)realloc( (int *)sumAsci->anzl, (sum+1)*sizeof(int)) ) == NULL )
    { printf("\n\nERROR: realloc failure in hashLine(), Line:%s not included\n\n", name); return(-1); }
    if ((sumAsci->lindx=(int **)realloc( (int **)sumAsci->lindx, (sum+1)*sizeof(int *)) ) == NULL )
    { printf("\n\nERROR: realloc failure in hashLine(), Line:%s not included\n\n", name); return(-1); }
    for(i=sumAsci->max_suml+1; i<=sum; i++) { sumAsci->anzl[i]=0; sumAsci->lindx[i]=NULL; }
    sumAsci->max_suml=sum;
  }
  else
  {
    if (delLineFlag)
    for (i=0; i<sumAsci->anzl[sum]; i++) 
    {
      n=sumAsci->lindx[sum][i];
      if( line[n].name == (char *)NULL )
      {
        /* already existing space to fill */
        sumAsci->lindx[sum][i]=nr;
        return(sum);
      }
    }
  }

  /* alloc of a new entry in the hash table */
  if ((sumAsci->lindx[sum]=(int *)realloc( (int *)sumAsci->lindx[sum], (sumAsci->anzl[sum]+1)*sizeof(int)) ) == NULL )
  { printf("\n\nERROR: realloc failure in hashLine(), line:%s not included\n\n", name); return(-1); }

  sumAsci->lindx[sum][sumAsci->anzl[sum]] = nr;
  sumAsci->anzl[sum]++;
  return(sum);
}


int line_i( char *name, int ip1, int ip2, int trk, int div, double bias, char type )
{
  static int nr, i;
  int sum=0;

  nr=getLineNr(name);
  if (nr==-1)        /* new line */
  {
    if ((line = (Lines *)realloc( (Lines *)line, (anzGeo->l+1)*sizeof(Lines)) ) == NULL )
    { printf("\n\nERROR: realloc failure in line, line:%s not installed\n\n", name); return(-1); }
    nr=anzGeo->l;
    i=strlen(name);
    if((line[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(line[nr].name, name);
    sum=hashLine( sumAsci, name, nr );
    anzGeo->l++;
    line[nr].div = ddiv;
    line[nr].nod = NULL;
    line[nr].elem = NULL;
    line[nr].ip= NULL;
  }
  else if (nr<-1)    /* replace a deleted line */
  {
    nr=-(nr+10);
    i=strlen(name);
    if((line[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(line[nr].name, name);
    sum=hashLine( sumAsci, name, nr );
    line[nr].div = ddiv;
  }
  else
  {
    if(printFlag) printf (" line %s in use: Definition will be changed\n", name);
    free(line[nr].nod);
    free(line[nr].elem);
    free(line[nr].ip);
    line[nr].nod = NULL;
    line[nr].elem = NULL;
    line[nr].ip= NULL;
  }

  if(div>0) line[nr].div = div;
  if(line[nr].div<1) line[nr].div = ddiv;
  line[nr].p1=ip1;
  line[nr].p2=ip2;
  line[nr].bias = bias;
  line[nr].etyp=0;
  line[nr].eattr=0;
  line[nr].nn= -1;
  line[nr].ne= -1;
  line[nr].nip= 0;
  if (type==0)
  {
    line[nr].trk=-1;
    line[nr].typ=' ';
  }
  else
  {
    if (type=='s') set[trk].type=1;
    line[nr].trk=trk;
    line[nr].typ=type;
  }

  for (i=0; i<anz->sets; i++)
  {
    if(( set[i].name != (char *)NULL)&&( set[i].flag=='o')) seta( i, "l", nr );
  }

  repLine(nr);
  return(nr);
}


int line_( char *name, char *p1, char *p2, char *trk, int div, double bias )
{
  static int nr, i;
  int ip1, ip2, sum=0;

  /* check if points exists */
  ip1=getPntNr(p1);
  if (ip1==-1)        /* new point */
  {
    printf ("ERROR: point %s of Line %s not defined\n", p1, name);
    return(-1);
  }
  ip2=getPntNr(p2);
  if (ip2==-1)        /* new point */
  {
    printf ("ERROR: point %s of Line %s not defined\n", p2, name);
    return(-1);
  }

  nr=getLineNr(name);
  if (nr==-1)        /* new line */
  {
    if ((line = (Lines *)realloc( (Lines *)line, (anzGeo->l+1)*sizeof(Lines)) ) == NULL )
    { printf("\n\nERROR: realloc failure in line, line:%s not installed\n\n", name); return(-1); }
    nr=anzGeo->l;
    i=strlen(name);
    if((line[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(line[nr].name, name);
    sum=hashLine( sumAsci, name, nr );
    anzGeo->l++;
    line[nr].div = ddiv;
    line[nr].nod = NULL;
    line[nr].elem = NULL;
    line[nr].ip= NULL;
  }
  else if (nr<-1)    /* replace a deleted line */
  {
    nr=-(nr+10);
    i=strlen(name);
    if((line[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(line[nr].name, name);
    sum=hashLine( sumAsci, name, nr );
    line[nr].div = ddiv; 
  }
  else
  {
    if(printFlag) printf (" line %s in use: Definition will be changed\n", name);
    free(line[nr].nod);
    free(line[nr].elem);
    free(line[nr].ip);
    line[nr].nod = NULL;
    line[nr].elem = NULL;
    line[nr].ip= NULL;
  }

  line[nr].p1=ip1;
  line[nr].p2=ip2;

  if (trk[0]!=' ')
  {
    i=getSetNr(trk);
    if (i>-1)        /* SEQ */
    {
      set[i].type=1;
      line[nr].trk=i;
      line[nr].typ='s';
    }
    else
    {
      i=getPntNr(trk);
      if (i>-1)        /* arc */
      {
        line[nr].trk=i;
        line[nr].typ='a';
      }
      else
      {
        printf ("ERROR: trk %s not defined, Line %s will be straight\n", trk, name);
        line[nr].trk=-1;
        line[nr].typ=' ';
      }
    }
  }
  else
  {
    line[nr].trk=-1;
    line[nr].typ=' ';
  }

  if(div>0) line[nr].div = div;
  if(line[nr].div<1) line[nr].div = ddiv;
  line[nr].bias = bias;
  line[nr].etyp=0;
  line[nr].eattr=0;
  line[nr].nn= -1;
  line[nr].ne= -1;
  line[nr].nip= 0;

  /* print the values of the hash table */
  if(sum<0) exit(-1);
  /*
  else printf(" asciSum:%d asci-indx:%d sumL in hash-table:%d name:%s index:%d\n"
  , sum, sumAsci->anzl[sum]-1, sumAsci->anzl[sum]
  , line[sumAsci->lindx[sum][sumAsci->anzl[sum]-1]].name
  , sumAsci->lindx[sum][sumAsci->anzl[sum]-1] );  
  */
  for (i=0; i<anz->sets; i++)
  {
    if(( set[i].name != (char *)NULL)&&( set[i].flag=='o')) seta( i, "l", nr );
  }

  repLine(nr);
  return(nr);
}


/* calculates cadfix representation of bias */
/* cadfix requires that a bias <=9 with no decimal-places is presented as integer */
/* all other are integer *10 */
int getBias_fbd(int l, Lines *line)
{
  int bias_fbd;

  if((line[l].bias!=1.)&&(line[l].div>1))
  {
    if(line[l].bias<1.)
    {
      bias_fbd= (pow(1./line[l].bias,(line[l].div-1.))*-10.) -.5;
    }
    else
    {
      bias_fbd= (pow(line[l].bias,(line[l].div-1.))*10.) +.5;
    }
  }
  else bias_fbd= 1;
  if((abs(bias_fbd)<100)&&((bias_fbd%10)==0)) bias_fbd/=10;
#if TEST
  printf("bias:%d\n", bias_fbd);
#endif
  return(bias_fbd);
}


void splitBiasDiv(int *ptrdiv, double *ptrbias)
{
  char datum[MAX_LINE_LENGTH];
  int div; 
  double bias;
  int bias_fbd;
  int i;

  div=*ptrdiv;
  bias=*ptrbias;


  /* determine bias and division */
  sprintf( datum, "%d", div);
  i=strlen( datum );
  if(i>2) 
  {
    div=atoi( &datum[i-2]);
    datum[i-2]=0;

    /* bias_fbd with more than one digit represents a float with one digit after the dot (x.x) * 10 as int */
    bias_fbd=atoi( datum);

    /* for conversation from old bias definition to the new one */
    if(old_bias_def)
      bias_fbd*=10;
    else
      if(abs(bias_fbd)<10) bias_fbd*=10;
    

#if TEST
  printf("div:%d bias_fbd:%d\n", div,bias_fbd);
#endif

    /* in the fbd-format-definition bias is defined as bias(a)=(elem_length(last)/elem_length(first)) */
    /* therefore a re-calculation is necessary because in cgx the definition is: */ 
    /* bias(b) = (elem_length(n+1)/elem_length(n)) => bias(a)= bias(b)**(div-1) */
    if((div>1)&&(abs(bias_fbd)>10))
    {
      if(bias_fbd<0) bias= 1./pow(((double)bias_fbd*-.1), (1./((double)div-1.)));
      else           bias= pow(((double)bias_fbd*.1), (1./((double)div-1.)));
    }
    else bias=1.;
  }
  else
  {
    bias=1.;
  }
  if (div>MAX_LINE_DIV) 
  {
    printf("WARNING: div:%d to high, reduced to %d\n",div, MAX_LINE_DIV);
    div=MAX_LINE_DIV;
  }

#if TEST
  printf("div:%d bias:%f\n", div,bias);
#endif

  *ptrdiv=div;
  *ptrbias=bias;
}

int pre_line( char *record, int addFlag)
{
  int  ii, i, j, length, div=0, lnr=-1;
  double bias;
  static char name[MAX_LINE_LENGTH], datum[4][MAX_LINE_LENGTH];
  int  p1, p2, trk=-1;
  int  buf1, buf2;
  char typ=0;

  if (!addFlag) length = sscanf( record, "%s%s%s%s%s", name, datum[0], datum[1], datum[2], datum[3] );
  else 
  {
    name[0]='!';
    datum[0][0]=datum[1][0]='%';
    length = sscanf( record, "%s%s%s%s%s", &name[1], &datum[0][1], &datum[1][1], datum[2], datum[3] );
  }

  operateAlias( name, "l" );
  operateAlias( datum[0], "p" );
  operateAlias( datum[1], "p" );

  /* check if a lcmb has this name, not use it */
  if (getLcmbNr(name) >0) {errMsg("ERROR: LCMB uses this name %s, try another name \n", name); return(-1);}

  if ( length==3 )
  {
    div=0;
    datum[2][0]=0;
  }
  else if ( length==4 )
  {
    /* check if we get a valid division from arg4 */
    div=atoi(datum[2]);
    if ((div>0)||(div<0))
    {
      datum[2][0]=0;
    }
    else
    {
      div=0;
      if (addFlag) { for(i=strlen(datum[2]); i>=0; i--) datum[2][i+1]=datum[2][i]; datum[2][0]='%'; }
      operateAlias( datum[2], "p" );
    }
  }
  else if ( length>4 )
  {
    div=atoi(datum[3]);
    if (addFlag) { for(i=strlen(datum[2]); i>=0; i--) datum[2][i+1]=datum[2][i]; datum[2][0]='%'; }
    operateAlias( datum[2], "p" );
  }
  else 
  {
    printf("ERROR: Inkorrect definition");
    return(-1);
  }

  if(printFlag) printf("line:%s %s %s %s %d \n", name, datum[0], datum[1], datum[2], div);

  /* check if points exists */
  p1=getPntNr(datum[0]);
  if (p1==-1)        /* new point */
  {
    printf ("ERROR: point %s of Line %s not defined\n", datum[0], name);
    return(-1);
  }
  p2=getPntNr(datum[1]);
  if (p2==-1)        /* new point */
  {
    printf ("ERROR: point %s of Line %s not defined\n", datum[1], name);
    return(-1);
  }
  if (datum[2][0])
  {
    i=getSetNr(datum[2]);
    if (i>-1)        /* SEQ */
    {
      /* if a point of the same name exists then check if the line endpoints are endpoints in the seq. if not use point. */ 
      ii=getPntNr(datum[2]);
      if (ii>-1)
      {
        if( ((p1!=set[i].pnt[0])&&(p2!=set[i].pnt[0]))
            || ((p1!=set[i].pnt[set[i].anz_p-1])&&(p2!=set[i].pnt[set[i].anz_p-1])) )
        {
          /* its not a seq */
          trk=ii;
          typ='a';
        }
      }
      if(typ==0)
      {
        set[i].type=1;
        trk=i;
        typ='s';

        /* special fix for bad defined seqa-lines */

        /* - add the endpoints if the endpoints are not included in the set */
        /* search for the endpoint in the set */
        buf1=buf2=-1; for(j=0; j<set[i].anz_p; j++) { if(p1==set[i].pnt[j]) buf1=j; if(p2==set[i].pnt[j]) buf2=j; }
        if((buf1==-1) && (buf2==-1))
	{
          if ( (set[i].pnt = (int *)realloc((int *)set[i].pnt, (set[i].anz_p+2)*sizeof(int))) == NULL )
            printf(" ERROR: malloc failed in setr\n\n");
          for(j=set[i].anz_p; j>0; j--)
          {
            set[i].pnt[j]=set[i].pnt[j-1];
          }
          set[i].pnt[0]=p1;
          set[i].anz_p++;

          if ( (set[i].pnt = (int *)realloc((int *)set[i].pnt, (set[i].anz_p+2)*sizeof(int))) == NULL )
            printf(" ERROR: malloc failed in setr\n\n");
          set[i].pnt[set[i].anz_p]=p2;
          set[i].anz_p++;
	}
        else if ((buf1==-1) || (buf2==-1))
	{
          printf("ERROR: just one endpoint in trk:%s, Line %s will be straight\n", set[i].name, name);
          trk=-1;
          typ=0;
	}
      }
      /* check if the seqence includes the end-points and has more than two points */
      if( ((p1!=set[i].pnt[0])&&(p2!=set[i].pnt[0]))
            || ((p1!=set[i].pnt[set[i].anz_p-1])&&(p2!=set[i].pnt[set[i].anz_p-1])) || (set[i].anz_p<3))
      {
        /* its not a valid seq */
        printf("ERROR: failure in trk:%s, Line %s will be straight\n", set[i].name, name);
        trk=-1;
        typ=0;
      }
    }
    else
    {
      i=getPntNr(datum[2]);
      if (i>-1)        /* arc */
      {
        trk=i;
        typ='a';
      }
      else
      {
        printf ("ERROR: trk %s not defined, Line %s will be straight\n", datum[2], name);
        trk=-1;
        typ=0;
      }
    }
  }
  else
  {
    trk=-1;
    typ=0;
  }

  /* determine bias and division (+conversion from old to new bias-definition) */
  old_bias_def=OLD_BIAS_DEF;

  splitBiasDiv(&div, &bias);

  old_bias_def=0;

  /* check the orientation of the sequence-set */
  if (typ=='s')
  {
    if (p2==set[trk].pnt[0])
    {
      printf("WARNING: sequence:%s in line:%s wrong defined, line inverted\n",set[trk].name,name);
      i=p2;
      p2=p1;
      p1=i;
    }
  }
  if( (lnr=line_i( name, p1, p2, trk, div, bias, typ)) <0)
    printf("ERROR: line could not be created\n");
  return(lnr);
}



/*------------------------------------------------------------------*/
/* define lcmb                                                      */
/*------------------------------------------------------------------*/

void delLcmb( int anzc, int *number )
{
  int j, k, *nrbuffer;

#if TEST
  printf("delLcmb\n");
#endif

  delLcmbFlag=1;
/*
  printf ("sum:%d num:%d lcmb:%s\n", anzc, number[0], lcmb[number[0]].name );
*/
  if ((nrbuffer = (int *)malloc((anzc+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in delLcmb\n\n"); return; }

  /* nessesary to store the numbers in a independent area  */
  for (j=0; j<anzc; j++)
    nrbuffer[j]=number[j];

  for (j=0; j<anzc; j++) if( lcmb[nrbuffer[j]].name != (char *)NULL )
  {
    for (k=0; k<anz->sets; k++)
    {
      if(set[k].type==0)
       if( set[k].name != (char *)NULL )
        setr( k, "c", nrbuffer[j]);
    }
    if(printFlag) printf (" delete lcmb:%s \n",  lcmb[nrbuffer[j]].name );
    free(lcmb[nrbuffer[j]].name);
    lcmb[nrbuffer[j]].name = (char *)NULL;
    lcmb[nrbuffer[j]].nl=0;
    free(lcmb[nrbuffer[j]].o);
    lcmb[nrbuffer[j]].o= NULL;
    free(lcmb[nrbuffer[j]].l);
    lcmb[nrbuffer[j]].l= NULL;
    lcmb[nrbuffer[j]].p1=-1;
    lcmb[nrbuffer[j]].p2=-1;
  }
  free(nrbuffer);
}


int hashLcmb( SumAsci *sumAsci, char *name, int nr)
{
  int i=0,j=0, n;
  int sum=0;

#if TEST
  printf("hashLcmb\n");
#endif

  while(name[i]!='\0') { sum+=name[i]*(++j); i++; }

  /* check if sum is higher as the allocated value */
  /* else look for a free entry */
  if(sum>sumAsci->max_sumc)
  {
    if ((sumAsci->anzc=(int *)realloc( (int *)sumAsci->anzc, (sum+1)*sizeof(int)) ) == NULL )
    { printf("\n\nERROR: realloc failure in hashLcmb(), Lcmb:%s not included\n\n", name); return(-1); }
    if ((sumAsci->cindx=(int **)realloc( (int **)sumAsci->cindx, (sum+1)*sizeof(int *)) ) == NULL )
    { printf("\n\nERROR: realloc failure in hashLcmb(), Lcmb:%s not included\n\n", name); return(-1); }
    for(i=sumAsci->max_sumc+1; i<=sum; i++) { sumAsci->anzc[i]=0; sumAsci->cindx[i]=NULL; }
    sumAsci->max_sumc=sum;
  }
  else
  {
    /* look for a free entry */
    if (delLcmbFlag)
    for (i=0; i<sumAsci->anzc[sum]; i++) 
    {
      n=sumAsci->cindx[sum][i];
      if( lcmb[n].name == (char *)NULL )
      {
        /* already existing space to fill */
        sumAsci->cindx[sum][i]=nr;
        return(sum);
      }
    }
  }

  /* alloc of a new entry in the hash table */
  if ((sumAsci->cindx[sum] 
  =(int *)realloc( (int *)sumAsci->cindx[sum], (sumAsci->anzc[sum]+1)*sizeof(int)) ) == NULL )
  { printf("\n\nERROR: realloc failure in hashLcmb(), lcmb:%s not included\n\n", name); return(-1); }

  sumAsci->cindx[sum][sumAsci->anzc[sum]] = nr;
  sumAsci->anzc[sum]++;
  return(sum);
}


int lcmb_i( char *name, int add, int anz_l, char  *ori, int *lin )
{
  static int nr, i;
  int sum=0, offset=0;

#if TEST
  printf("lcmb_i\n");
#endif

  nr=getLcmbNr(name);
  if (nr==-1)        /* new lcmb */
  {
    if ((lcmb = (Lcmb *)realloc( (Lcmb *)lcmb, (anzGeo->c+1)*sizeof(Lcmb)) ) == NULL )
     { printf("\n\nERROR: realloc failure in lcmb_, lcmb:%s not installed\n\n", name); return(-1); }
    nr=anzGeo->c;
    if ((lcmb[nr].o = (char *)malloc( (anz_l+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: malloc failure in lcmb_, lcmb.o:%s not installed\n\n", name); return(-1); }
    if ((lcmb[nr].l = (int *)malloc( (anz_l+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: malloc failure in lcmb_, lcmb.l:%s not installed\n\n", name); return(-1); }
    anzGeo->c++;
    i=strlen(name);
    if((lcmb[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(lcmb[nr].name, name);
    sum=hashLcmb( sumAsci, name, nr );
  }
  else if (nr<-1)    /* replace a deleted lcmb */
  {
    nr=-(nr+10);
    if ((lcmb[nr].o = (char *)realloc( (char *)lcmb[nr].o, (anz_l+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: realloc failure in lcmb_, lcmb.o :%s not installed\n\n", name); return(-1); }
    if ((lcmb[nr].l = (int *)realloc( (int *)lcmb[nr].l, (anz_l+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in lcmb_, lcmb.l:%s not installed\n\n", name); return(-1); }
    i=strlen(name);
    if((lcmb[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(lcmb[nr].name, name);
    sum=hashLcmb( sumAsci, name, nr );
  }
  else
  {
    if(add) offset=lcmb[nr].nl;
    /* check the number of lines */
    if(printFlag) printf (" lcmb in use: Definition will be changed\n");
    if ((lcmb[nr].o = (char *)realloc( (char *)lcmb[nr].o, (anz_l+offset+1)*sizeof(char)) ) == NULL )
    { printf("\n\nERROR: realloc failure in lcmb_, lcmb.o:%s not installed\n\n", name); return(-1); }
    if ((lcmb[nr].l = (int *)realloc( (int *)lcmb[nr].l, (anz_l+offset+1)*sizeof(int)) ) == NULL )
    { printf("\n\nERROR: realloc failure in lcmb_, lcmb.l:%s not installed\n\n", name); return(-1); }
  }
  for (i=0; i<anz_l; i++)
  {
    lcmb[nr].l[i+offset]=lin[i];
    lcmb[nr].o[i+offset]=ori[i];
  }
  lcmb[nr].nl= anz_l+offset;

  /* print the values of the hash table */
  if(sum<0) exit(-1);
  /*
  else printf(" asciSum:%d asci-indx:%d sumC in hash-table:%d name:%s index:%d\n"
  , sum, sumAsci->anzc[sum]-1, sumAsci->anzc[sum]
  , lcmb[sumAsci->cindx[sum][sumAsci->anzc[sum]-1]].name
  , sumAsci->cindx[sum][sumAsci->anzc[sum]-1] );  
  */

  /* orient the new lcmb */
  if ( orientLcmb( nr ) <0 )
  {
      errMsg ("ERROR: orientLcmb:%s failed\n", name);
      delLcmb( 1, &nr );
      return(-1);
  }
  for (i=0; i<anz->sets; i++)
  {
    if(( set[i].name != (char *)NULL)&&( set[i].flag=='o')) seta( i, "c", nr );
  }
  return(nr);
}


int lcmb_( char *name, int add, int anz_l, char *ori, char *lin )
{
  static int nr, i, lnr;
  int sum=0, offset=0;

#if TEST
  printf("lcmb_\n");
#endif

  nr=getLcmbNr(name);
  if (nr==-1)        /* new lcmb */
  {
    if ((lcmb = (Lcmb *)realloc( (Lcmb *)lcmb, (anzGeo->c+1)*sizeof(Lcmb)) ) == NULL )
     { printf("\n\nERROR: realloc failure in lcmb_, lcmb:%s not installed\n\n", name); return(-1); }
    nr=anzGeo->c;
    if ((lcmb[nr].o = (char *)malloc( (anz_l+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: malloc failure in lcmb_, lcmb.o:%s not installed\n\n", name); return(-1); }
    if ((lcmb[nr].l = (int *)malloc( (anz_l+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: malloc failure in lcmb_, lcmb.l:%s not installed\n\n", name); return(-1); }
    anzGeo->c++;
    i=strlen(name);
    if((lcmb[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(lcmb[nr].name, name);
    sum=hashLcmb( sumAsci, name, nr );
  }
  else if (nr<-1)    /* replace a deleted lcmb */
  {
    nr=-(nr+10);
    if ((lcmb[nr].o = (char *)realloc( (char *)lcmb[nr].o, (anz_l+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: realloc failure in lcmb_, lcmb.o :%s not installed\n\n", name); return(-1); }
    if ((lcmb[nr].l = (int *)realloc( (int *)lcmb[nr].l, (anz_l+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in lcmb_, lcmb.l:%s not installed\n\n", name); return(-1); }
    i=strlen(name);
    if((lcmb[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(lcmb[nr].name, name);
    sum=hashLcmb( sumAsci, name, nr );
  }
  else
  {
    if(add) offset=lcmb[nr].nl;
    if(printFlag) printf (" lcmb in use: Definition will be changed\n");
    if ((lcmb[nr].o = (char *)realloc( (char *)lcmb[nr].o, (anz_l+offset+1)*sizeof(char)) ) == NULL )
    { printf("\n\nERROR: realloc failure in lcmb_, lcmb.o:%s not installed\n\n", name); return(-1); }
    if ((lcmb[nr].l = (int *)realloc( (int *)lcmb[nr].l, (anz_l+offset+1)*sizeof(int)) ) == NULL )
    { printf("\n\nERROR: realloc failure in lcmb_, lcmb.l:%s not installed\n\n", name); return(-1); }
  }
  for (i=0; i<anz_l; i++)
  {
    lnr=getLineNr((char*)&lin[i*MAX_LINE_LENGTH]);
    if( lnr>-1 ) { lcmb[nr].l[i+offset]=lnr; }
    else
    {
      errMsg ("ERROR: line:%s in lcmb:%s are undefined\n", &lin[i*MAX_LINE_LENGTH], lcmb[nr].name );
      if(printFlag) printf (" delete lcmb:%s \n",  lcmb[nr].name );
      lcmb[nr].name = (char *)NULL ;
      return(-1);
    }
    lcmb[nr].o[i+offset]=ori[i*2];
  }
  lcmb[nr].nl= anz_l+offset;

  /* print the values of the hash table */
  if(sum<0) exit(-1);
  /*
  else printf(" asciSum:%d asci-indx:%d sumC in hash-table:%d name:%s index:%d\n"
  , sum, sumAsci->anzc[sum]-1, sumAsci->anzc[sum]
  , lcmb[sumAsci->cindx[sum][sumAsci->anzc[sum]-1]].name
  , sumAsci->cindx[sum][sumAsci->anzc[sum]-1] );  
  */

  /* orient the new lcmb */
  if ( orientLcmb( nr ) <0 )
  {
      errMsg ("ERROR: orientLcmb:%s failed\n", name);
      delLcmb( 1, &nr );
      return(-1);
  }
  for (i=0; i<anz->sets; i++)
  {
    if(( set[i].name != (char *)NULL)&&( set[i].flag=='o')) seta( i, "c", nr );
  }
  return(nr);
}


void pre_lcmb( char *record, int addFlag)
{
  int  i,j, length, anz_l, add;
  static char name[MAX_LINE_LENGTH], typ[MAX_LINE_LENGTH];
  static char ori[14][2], lin[14][MAX_LINE_LENGTH];

#if TEST
  printf("pre_lcmb\n");
#endif

  if (!addFlag) length = sscanf( record, "%s%s", name, typ);
  else
  {
    length = sscanf( record, "%s%s", &name[1], typ);
    if( compare(typ, "ADD", 3) == 3) name[0]='%'; else  name[0]='!';
  }
  operateAlias( name, "c" );

  /* check if a line has this name, not use it */
  if (getLineNr(name) >0) { errMsg("ERROR: Could not create lcmb:%s A LINE uses this name already\n", name); return; }

  if( compare(typ, "ADD", 3) == 3)
  {
    add=1;
    length = sscanf( record, "%*s%*s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
     ori[0], lin[0],ori[1], lin[1],
     ori[2], lin[2],ori[3], lin[3],ori[4], lin[4],ori[5], lin[5],ori[6], lin[6],ori[7], lin[7],
     ori[8], lin[8],ori[9], lin[9],ori[10], lin[10],ori[11], lin[11],ori[12], lin[12],ori[13], lin[13] );
  }
  else
  {
    add=0;
    length = sscanf( record, "%*s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", 
     ori[0], lin[0],ori[1], lin[1],
     ori[2], lin[2],ori[3], lin[3],ori[4], lin[4],ori[5], lin[5],ori[6], lin[6],ori[7], lin[7],
     ori[8], lin[8],ori[9], lin[9],ori[10], lin[10],ori[11], lin[11],ori[12], lin[12],ori[13], lin[13] );
  }
  anz_l=length/2;
  if(printFlag) printf("lcmb: %s ",  name );
  for (i=0; i<anz_l; i++)
  {
    if (addFlag) { for(j=strlen(lin[i]); j>=0; j--) lin[i][j+1]=lin[i][j]; lin[i][0]='%'; }
    operateAlias( lin[i], "l" );
    if ((ori[i][0]=='+')||(ori[i][0]=='-'))
    {
      if(printFlag) printf("%s %s ", ori[i], lin[i]);
    }
    else
    {
      printf("ERROR: Orientation:%s of line:%s not recognized\n", ori[i], lin[i] );
      return;
    }
  }
  if(printFlag) printf("\n");
  i=lcmb_( name, add, anz_l, &ori[0][0], &lin[0][0] );
  if( i <0)
  {
    printf("ERROR: lcmb could not be created\n");
    pre_seta( specialset->zap, "c", name);
    zap(specialset->zap);
  }
}


/*------------------------------------------------------------------*/
/* define surface                                                   */
/*------------------------------------------------------------------*/

void delSurf( int anzc, int *number )
{
  int j, k, *nrbuffer;

  delSurfFlag=1;
/*
  printf ("sum:%d num:%d surf:%s\n", anzc, number[0], surf[number[0]].name );
*/
  if ((nrbuffer = (int *)malloc((anzc+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in delSurf\n\n"); return; }

  /* nessesary to store the numbers in a independent area  */
  for (j=0; j<anzc; j++)
    nrbuffer[j]=number[j];

  for (j=0; j<anzc; j++) if( surf[nrbuffer[j]].name != (char *)NULL )
  {
    for (k=0; k<anz->sets; k++)
    {
      if(set[k].type==0)
       if( set[k].name != (char *)NULL )
        setr( k, "s", nrbuffer[j]);
    }
    if(printFlag) printf (" delete surf:%s \n",  surf[nrbuffer[j]].name );
    free(surf[nrbuffer[j]].name);
    surf[nrbuffer[j]].name = (char *)NULL ;
    surf[nrbuffer[j]].nl= 0;
    free(surf[nrbuffer[j]].typ);
    surf[nrbuffer[j]].typ= NULL;
    free(surf[nrbuffer[j]].o);
    surf[nrbuffer[j]].o= NULL;
    free(surf[nrbuffer[j]].l);
    surf[nrbuffer[j]].l= NULL;
    surf[nrbuffer[j]].nn= -1;
    free(surf[nrbuffer[j]].nod);
    surf[nrbuffer[j]].nod= NULL;
    surf[nrbuffer[j]].ne= -1;
    free(surf[nrbuffer[j]].elem);
    surf[nrbuffer[j]].elem= NULL;
    surf[nrbuffer[j]].etyp= 0;
    surf[nrbuffer[j]].npgn= 0;
    free(surf[nrbuffer[j]].pgn);
    surf[nrbuffer[j]].pgn= NULL;
  }
  free(nrbuffer);
}


int hashSurf( SumAsci *sumAsci, char *name, int nr)
{
  int i=0,j=0, n;
  int sum=0;

  while(name[i]!='\0') { sum+=name[i]*(++j); i++; }

  /* check if sum is higher as the allocated value */
  /* else look for a free entry */
  if(sum>sumAsci->max_sums)
  {
    if ((sumAsci->anzs=(int *)realloc( (int *)sumAsci->anzs, (sum+1)*sizeof(int)) ) == NULL )
    { printf("\n\nERROR: realloc failure in hashSurf(), Surf:%s not included\n\n", name); return(-1); }
    if ((sumAsci->sindx=(int **)realloc( (int **)sumAsci->sindx, (sum+1)*sizeof(int *)) ) == NULL )
    { printf("\n\nERROR: realloc failure in hashSurf(), Surf:%s not included\n\n", name); return(-1); }
    for(i=sumAsci->max_sums+1; i<=sum; i++) { sumAsci->anzs[i]=0; sumAsci->sindx[i]=NULL; }
    sumAsci->max_sums=sum;
  }
  else
  {
    if (delSurfFlag)
    for (i=0; i<sumAsci->anzs[sum]; i++) 
    {
      n=sumAsci->sindx[sum][i];
      if( surf[n].name == (char *)NULL )
      {
        /* already existing space to fill */
        sumAsci->sindx[sum][i]=nr;
        return(sum);
      }
    }
  }

  /* alloc of a new entry in the hash table */
  if ((sumAsci->sindx[sum] 
  =(int *)realloc( (int *)sumAsci->sindx[sum], (sumAsci->anzs[sum]+1)*sizeof(int)) ) == NULL )
  { printf("\n\nERROR: realloc failure in hashSurf(), surf:%s not included\n\n", name); return(-1); }

  sumAsci->sindx[sum][sumAsci->anzs[sum]] = nr;
  sumAsci->anzs[sum]++;
  return(sum);
}


int  surface_i( char *name, char ori, int blend, int anz_c, char *cori, int *edge, char *ctyp )
{
  static int nr, i;
  int sum=0;
  nr=getSurfNr(name);
  if (nr==-1)        /* new surf */
  {
    if ((surf = (Gsur *)realloc( (Gsur *)surf, (anzGeo->s+1)*sizeof(Gsur)) ) == NULL )
     { printf("\n\nERROR: realloc failure in surface, surf:%s not installed\n\n", name); return(-1); }
    nr=anzGeo->s;
    if ((surf[nr].typ = (char *)malloc( (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: malloc failure in surface, surf.typ:%s not installed\n\n", name); return(-1); }
    if ((surf[nr].o = (char *)malloc( (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: malloc failure in surface, surf.o:%s not installed\n\n", name); return(-1); }
    if ((surf[nr].l = (int *)malloc( (anz_c+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: malloc failure in surface, surf.l:%s not installed\n\n", name); return(-1); }
    anzGeo->s++;
    i=strlen(name);
    if((surf[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(surf[nr].name, name);
    sum=hashSurf( sumAsci, name, nr );
    surf[nr].nc= 0;
    surf[nr].nn= -1;
    surf[nr].ne= -1;
    surf[nr].c= NULL;
    surf[nr].nod= NULL;
    surf[nr].elem= NULL;
    surf[nr].npgn= 0;
    surf[nr].pgn= NULL;
  }
  else if (nr<-1)    /* replace a deleted surf */
  {
    nr=-(nr+10);
    if ((surf[nr].typ = (char *)realloc( (char *)surf[nr].typ, (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: realloc failure in surface, surf.typ:%s not installed\n\n", name); return(-1); }
    if ((surf[nr].o = (char *)realloc( (char *)surf[nr].o, (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: realloc failure in surface, surf.o:%s not installed\n\n", name); return(-1); }
    if ((surf[nr].l = (int *)realloc( (int *)surf[nr].l, (anz_c+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in surface, surf.l:%s not installed\n\n", name); return(-1); }
    i=strlen(name);
    if((surf[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(surf[nr].name, name);
    sum=hashSurf( sumAsci, name, nr );
  }
  else
  {
    if(printFlag) printf (" surf in use: Definition will be changed\n");
    if ((surf[nr].typ = (char *)realloc( (char *)surf[nr].typ, (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: realloc failure in surface, surf.typ:%s not installed\n\n", name); return(-1); }
    if ((surf[nr].o = (char *)realloc( (char *)surf[nr].o, (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: realloc failure in surface, surf.o:%s not installed\n\n", name); return(-1); }
    if ((surf[nr].l = (int *)realloc( (int *)surf[nr].l, (anz_c+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in surface, surf.l:%s not installed\n\n", name); return(-1); }
    free(surf[nr].c);
    free(surf[nr].nod);
    free(surf[nr].elem);
    surf[nr].c= NULL;
    surf[nr].nod= NULL;
    surf[nr].elem= NULL;
    surf[nr].nc= 0;
    surf[nr].nn= -1;
    surf[nr].ne= -1;
    surf[nr].npgn= 0;
    free(surf[nr].pgn);
    surf[nr].pgn= NULL;
  }

  for (i=0; i<anz_c; i++)
  {
    surf[nr].typ[i]= ctyp[i];
    surf[nr].l[i]  = edge[i];
    surf[nr].o[i]  = cori[i];
  }
  surf[nr].sh= blend;
  surf[nr].ori= ori;
  surf[nr].nl= anz_c;
  surf[nr].etyp=0;
  surf[nr].eattr=0;

  /* print the values of the hash table */
  if(sum<0) exit(-1);
  /*
  else printf(" asciSum:%d asci-indx:%d sums in hash-table:%d name:%s index:%d\n"
  , sum, sumAsci->anzs[sum]-1, sumAsci->anzs[sum]
  , surf[sumAsci->sindx[sum][sumAsci->anzs[sum]-1]].name
  , sumAsci->sindx[sum][sumAsci->anzs[sum]-1] );  
  */

  /* orient the new surf */
  if ( orientSurf( nr ) <0 )
  {
    errMsg ("ERROR: orientSurf:%s failed\n", name);
    surf[nr].name = (char *)NULL ;
    return(-1);
  }
  for (i=0; i<anz->sets; i++)
  {
    if(( set[i].name != (char *)NULL)&&( set[i].flag=='o')) seta( i, "s", nr );
  }
  return(nr);
}

int  surface( char *name, char ori, char *blend, int anz_c, char *cori, char *edge )
{
  static int nr, i, lnr, cnr;
  int sum=0;

  nr=getSurfNr(name);
  if (nr==-1)        /* new surf */
  {
    if ((surf = (Gsur *)realloc( (Gsur *)surf, (anzGeo->s+1)*sizeof(Gsur)) ) == NULL )
     { printf("\n\nERROR: realloc failure in surface, surf:%s not installed\n\n", name); return(-1); }
    nr=anzGeo->s;
    if ((surf[nr].typ = (char *)malloc( (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: malloc failure in surface, surf.typ:%s not installed\n\n", name); return(-1); }
    if ((surf[nr].o = (char *)malloc( (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: malloc failure in surface, surf.o:%s not installed\n\n", name); return(-1); }
    if ((surf[nr].l = (int *)malloc( (anz_c+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: malloc failure in surface, surf.l:%s not installed\n\n", name); return(-1); }
    anzGeo->s++;
    i=strlen(name);
    if((surf[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(surf[nr].name, name);
    sum=hashSurf( sumAsci, name, nr );
    surf[nr].nc= 0;
    surf[nr].nn= -1;
    surf[nr].ne= -1;
    surf[nr].c= NULL;
    surf[nr].nod= NULL;
    surf[nr].elem= NULL;
    surf[nr].npgn= 0;
    surf[nr].pgn= NULL;
  }
  else if (nr<-1)    /* replace a deleted surf */
  {
    nr=-(nr+10);
    if ((surf[nr].typ = (char *)realloc( (char *)surf[nr].typ, (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: realloc failure in surface, surf.typ:%s not installed\n\n", name); return(-1); }
    if ((surf[nr].o = (char *)realloc( (char *)surf[nr].o, (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: realloc failure in surface, surf.o:%s not installed\n\n", name); return(-1); }
    if ((surf[nr].l = (int *)realloc( (int *)surf[nr].l, (anz_c+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in surface, surf.l:%s not installed\n\n", name); return(-1); }
    i=strlen(name);
    if((surf[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(surf[nr].name, name);
    sum=hashSurf( sumAsci, name, nr );
  }
  else
  {
    if(printFlag) printf (" surf in use: Definition will be changed\n");
    if ((surf[nr].typ = (char *)realloc( (char *)surf[nr].typ, (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: realloc failure in surface, surf.typ:%s not installed\n\n", name); return(-1); }
    if ((surf[nr].o = (char *)realloc( (char *)surf[nr].o, (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: realloc failure in surface, surf.o:%s not installed\n\n", name); return(-1); }
    if ((surf[nr].l = (int *)realloc( (int *)surf[nr].l, (anz_c+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in surface, surf.l:%s not installed\n\n", name); return(-1); }
    free(surf[nr].c);
    free(surf[nr].nod);
    free(surf[nr].elem);
    surf[nr].c= NULL;
    surf[nr].nod= NULL;
    surf[nr].elem= NULL;
    surf[nr].nc= 0;
    surf[nr].nn= -1;
    surf[nr].ne= -1;
    surf[nr].npgn= 0;
    free(surf[nr].pgn);
    surf[nr].pgn= NULL;
  }

  for (i=0; i<anz_c; i++)
  {
    operateAlias( (char*)&edge[i*MAX_LINE_LENGTH], "l" );
    lnr=getLineNr((char*)&edge[i*MAX_LINE_LENGTH]);
    operateAlias( (char*)&edge[i*MAX_LINE_LENGTH], "c" );
    cnr=getLcmbNr((char*)&edge[i*MAX_LINE_LENGTH]);
    if( lnr>-1 ) { surf[nr].typ[i]='l'; surf[nr].l[i]=lnr; }
    else if( cnr>-1 ) { surf[nr].typ[i]='c'; surf[nr].l[i]=cnr; }
    else
    {
      errMsg ("ERROR: line:%s in surf:%s are undefined\n", &edge[i*MAX_LINE_LENGTH], name );
      if(printFlag) printf (" delete surf:%s \n",  surf[nr].name );
      surf[nr].name = (char *)NULL ;
      return(-1);
    }
    surf[nr].o[i]=cori[i*2];
  }
  operateAlias( blend, "S" );
  surf[nr].sh= getShapeNr(blend);
  surf[nr].ori= ori;
  surf[nr].nl= anz_c;
  surf[nr].etyp=0;
  surf[nr].eattr=0;

  /* print the values of the hash table */
  if(sum<0) exit(-1);
  /*
  else printf(" asciSum:%d asci-indx:%d sums in hash-table:%d name:%s index:%d\n"
  , sum, sumAsci->anzs[sum]-1, sumAsci->anzs[sum]
  , surf[sumAsci->sindx[sum][sumAsci->anzs[sum]-1]].name
  , sumAsci->sindx[sum][sumAsci->anzs[sum]-1] );  
  */

  /* orient the new surf */
  if ( orientSurf( nr ) <0 )
  {
    errMsg ("WARNING: orientSurf:%s failed\n", name);
  }
  for (i=0; i<anz->sets; i++)
  {
    if(( set[i].name != (char *)NULL)&&( set[i].flag=='o')) seta( i, "s", nr );
  }
  return(nr);
}


int getSurfParameters( char *record, char *name, char *ori, char *blend, char **cori,  char **edge, int offset )
{
  int i=0;  /* scans through the record */
  int j,k;
  char *cori_, *edge_;

  /* take the addresses from the calling function */
  /* before the first call, the calling function must deliver NULL */
  cori_=*cori;
  edge_=*edge;

  
  /* read the name */
  do
  {
    if(record[i]!=' ') break;
    i++;
  }while(1);
  for(j=offset; j<MAX_LINE_LENGTH; j++)
  {
    name[j]=record[i];
    i++;
    if(record[i]==' ') break;
  }
  name[j+1]='\0';
 
  /* read either orientation of the surface or 'ADD' */
  do
  {
    i++;
    if(record[i]!=' ') break;
  }while(1);
  for(j=0; j<MAX_LINE_LENGTH; j++)
  {
    ori[j]=record[i];
    i++;
    if(record[i]==' ') break;
  }
  ori[j+1]='\0';

  /* read either the BLEND or if ori=="ADD" then no blend is specified */
  if((ori[0]=='+')||(ori[0]=='-'))
  {
    do
    {
      i++;
      if(record[i]!=' ') break;
    }while(1);
    for(j=offset; j<MAX_LINE_LENGTH; j++)
    {
      blend[j]=record[i];
      i++;
      if(record[i]==' ') break;
    }
    blend[j+1]='\0';
  }
  
  /* get the orientation- and the name of the lines */
  j=-1;
  do
  {
    /* get the orientation and scan for eor */
    do
    {
      if( (record[i]=='\n') || (record[i]=='\r') ) goto found_all_lines;
      if(record[i]==(char)EOF) goto found_all_lines;
      if(record[i]=='\0') goto found_all_lines;
      if(record[i]!=' ') break;
      i++;
    }while(1);

    /* new line expected */
    j++;

    cori_=(char *)realloc((char *)cori_, (int)((j+2)*2)*sizeof(char));
    if(cori_==NULL) { printf("ERROR: realloc failed in getSurfParameters()\n"); return(0); }
    cori_[j*2+1]='\0';
    cori_[j*2]=record[i];

    /* get the name */
    do
    {
      i++;
      if(record[i]!=' ') break;
    }while(1);     
    for(k=offset; k<MAX_LINE_LENGTH; k++)
    {
      edge_=(char *)realloc((char *)edge_, (int)((j+2)*MAX_LINE_LENGTH)*sizeof(char));
      if(edge_==NULL) { printf("ERROR: realloc failed in getSurfParameters()\n"); return(0); }
      edge_[j*MAX_LINE_LENGTH+k]=record[i];
      i++;
      if(record[i]==' ') break;
     if( (record[i]=='\n') || (record[i]=='\r') ) break;
      if(record[i]==(char)EOF) break;
      if(record[i]=='\0') break;
    }
    edge_[j*MAX_LINE_LENGTH+k+1]='\0';

    /* next line j */
  }while(1);

 found_all_lines:;
  *cori=cori_;
  *edge=edge_;
  return(j+1);
}
   
  

void pre_gsur( char *record, int addFlag)
{
  int    anz_c, i, nr, lines;
  char name[MAX_LINE_LENGTH], ori[MAX_LINE_LENGTH], blend[MAX_LINE_LENGTH];
  static char  *cori=NULL, *edge=NULL;

  if(addFlag)
  {
    /* add a special char in front of all strings */
    anz_c = getSurfParameters(record, name, ori, blend, &cori, &edge, (int)1);
    if(compareStrings(ori, "ADD")==3) name[0]='%'; else  name[0]='!';
    blend[0]='%';
    for(i=0; i<anz_c; i++) edge[i*MAX_LINE_LENGTH]='%';
  }
  else
    anz_c = getSurfParameters(record, name, ori, blend, &cori, &edge, (int)0);
 

  operateAlias( name, "s " );
  if(printFlag) printf("Gsur:%s ", name );


  /* check if lines should be added to an existing surface. This is triggered by the */
  /* keyword 'ADD' in parameter ori. If yes then add the already defined lines to the */
  /* new ones */
  if(compareStrings(ori, "ADD")==3)
  {
    /* get the index of the original surface */
    nr=getSurfNr(name);
    if (nr==-1)
    {
      printf("ERROR: in pre_gsur, surf:%s does not exist and can not be extended\n", name);
      return;
    }

    /* get the ori and blend from the original surface */
    ori[0]=surf[nr].ori;
    ori[1]='\0';
    if(surf[nr].sh>-1) strcpy(blend,shape[surf[nr].sh].name);
    else strcpy(blend,"BLEND");

    /* extend cori and edge by the already defined ones */
    lines=anz_c+surf[nr].nl;
    cori=(char *)realloc((char *)cori, (int)(lines*2)*sizeof(char));
    if(cori==NULL) { printf("ERROR: realloc failed in pre_gsur()\n"); return; }
    edge=(char *)realloc((char *)edge, (int)(lines*MAX_LINE_LENGTH)*sizeof(char));
    if(edge==NULL) { printf("ERROR: realloc failed in pre_gsur()\n"); return; }

    /* re-arrange and store all lines */
    for(i=anz_c-1; i>-1; i--)
    {
      strcpy(&cori[(i+surf[nr].nl)*2], &cori[i*2]);
      strcpy(&edge[(i+surf[nr].nl)*MAX_LINE_LENGTH], &edge[i*MAX_LINE_LENGTH]);
    }
    for(i=0; i<surf[nr].nl; i++)
    {
      cori[i*2]= surf[nr].o[i];
      cori[i*2+1]= '\0';
      if(surf[nr].typ[i]=='l') strcpy(&edge[i*MAX_LINE_LENGTH], line[surf[nr].l[i]].name);
      if(surf[nr].typ[i]=='c') strcpy(&edge[i*MAX_LINE_LENGTH], lcmb[surf[nr].l[i]].name);
    }
    anz_c=lines;
  }

  if ((ori[0]=='+')||(ori[0]=='-'))
  {
    if(printFlag) printf("%s %s ", ori, blend);
  }
  else
  {
    printf("ERROR: Orientation:%s of surf:%s not recognized\n", ori, name );
    return;
  }

  for (i=0; i<anz_c; i++)
  {
    if ((compare( &cori[i*2], "+", 1) ==1 )||(compare( &cori[i*2], "-", 1) ==1 ))
    {
      if(printFlag) printf("%s %s ", &cori[i*2], &edge[i*MAX_LINE_LENGTH]);
    }
    else
    {
      printf("ERROR: Orientation:%s of line:%s not recognized\n", &cori[i*2], &edge[i*MAX_LINE_LENGTH] );
      return;
    }
  }
  if(printFlag) printf("\n");

  if( surface( name, ori[0], blend, anz_c, &cori[0], &edge[0] ) <0)
    printf("ERROR: surface could not be created\n");
}


void pre_surf( char *record)
{
  int  length, anz_c, i, se;
  static char name[MAX_LINE_LENGTH], ori[2], blend[MAX_LINE_LENGTH], cori[6][2], edge[6][MAX_LINE_LENGTH];
  char *dummy=NULL;
  strcpy(blend,"BLEND");

  length = sscanf( record, "%s%s%s%s%s%s%s", name, edge[0], edge[1], edge[2], edge[3], edge[4], edge[5] );
  operateAlias( name, "s " );

  anz_c=length-1;
  if ((anz_c > 5)||(anz_c < 3))
  {
    se=getSetNr( edge[0]);
    if(se>-1)
    {
      /* a set is defined, generate an unstructured surface */
      dummy=(char *)realloc((char *)dummy, (int)(MAX_LINE_LENGTH)*sizeof(char));
      if(edge==NULL) { printf("ERROR: realloc failed in pre_surf()\n"); }

      sprintf(dummy,"%s + %s", name,blend);
      for(i=0; i<set[se].anz_l; i++)
      {
        dummy=(char *)realloc((char *)dummy, (int)(strlen(dummy)+strlen(line[set[se].line[i]].name)+10)*sizeof(char));
        if(edge==NULL) { printf("ERROR: realloc failed in pre_surf()\n"); }

        sprintf(&dummy[strlen(dummy)]," + %s", line[set[se].line[i]].name);
      }
      pre_gsur( dummy, 0 );
      free(dummy);
      return;
    }
    printf( "ERROR: surf:%s has %d edges defined but must have between 3 and 5 \n", name, anz_c );
    return;
  }

  ori[0]='+';
  for (i=0; i<anz_c; i++)
  {
    cori[i][0]='+';
  }

  if( surface( name, ori[0], blend, anz_c, &cori[0][0], &edge[0][0] ) <0)
    printf("ERROR: surface could not be created\n");
}


/*------------------------------------------------------------------*/
/* define body                                                      */
/*------------------------------------------------------------------*/

void delBody( int anzb, int *number )
{
  int j, k, *nrbuffer;

  delBodyFlag=1;
/*
  printf ("sum:%d num:%d surf:%s\n", anzb, number[0], surf[number[0]].name );
*/
  if ((nrbuffer = (int *)malloc((anzb+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in delBody\n\n"); return; }

  /* nessesary to store the numbers in a independent area  */
  for (j=0; j<anzb; j++)
    nrbuffer[j]=number[j];

  for (j=0; j<anzb; j++) if( body[nrbuffer[j]].name != (char *)NULL )
  {
    for (k=0; k<anz->sets; k++)
    {
      if(set[k].type==0)
       if( set[k].name != (char *)NULL )
        setr( k, "b", nrbuffer[j]);
    }
    if(printFlag) printf (" delete body:%s \n",  body[nrbuffer[j]].name );
    free(body[nrbuffer[j]].name);
    body[nrbuffer[j]].name = (char *)NULL ;
    body[nrbuffer[j]].ns=0;
    free(body[nrbuffer[j]].o);
    body[nrbuffer[j]].o= NULL;
    free(body[nrbuffer[j]].s);
    body[nrbuffer[j]].s= NULL;
    body[nrbuffer[j]].nn=-1;
    free(body[nrbuffer[j]].nod);
    body[nrbuffer[j]].nod= NULL;
    body[nrbuffer[j]].ne=-1;
    free(body[nrbuffer[j]].elem);
    body[nrbuffer[j]].elem= NULL;      
    body[nrbuffer[j]].etyp= 0;
  }
  free(nrbuffer);
}


int hashBody( SumAsci *sumAsci, char *name, int nr)
{
  int i=0,j=0, n;
  int sum=0;

  while(name[i]!='\0') { sum+=name[i]*(++j); i++; }

  /* check if sum is higher as the allocated value */
  /* else look for a free entry */
  if(sum>sumAsci->max_sumb)
  {
    if ((sumAsci->anzb=(int *)realloc( (int *)sumAsci->anzb, (sum+1)*sizeof(int)) ) == NULL )
    { printf("\n\nERROR: realloc failure in hashBody(), Body:%s not included\n\n", name); return(-1); }
    if ((sumAsci->bindx=(int **)realloc( (int **)sumAsci->bindx, (sum+1)*sizeof(int *)) ) == NULL )
    { printf("\n\nERROR: realloc failure in hashBody(), Body:%s not included\n\n", name); return(-1); }
    for(i=sumAsci->max_sumb+1; i<=sum; i++) { sumAsci->anzb[i]=0; sumAsci->bindx[i]=NULL; }
    sumAsci->max_sumb=sum;
  }
  else
  {
    if (delBodyFlag)
    for (i=0; i<sumAsci->anzb[sum]; i++) 
    {
      n=sumAsci->bindx[sum][i];
      if( body[n].name == (char *)NULL )
      {
        /* already existing space to fill */
        sumAsci->bindx[sum][i]=nr;
        return(sum);
      }
    }
  }

  /* alloc of a new entry in the hash table */
  if ((sumAsci->bindx[sum] 
  =(int *)realloc( (int *)sumAsci->bindx[sum], (sumAsci->anzb[sum]+1)*sizeof(int)) ) == NULL )
  { printf("\n\nERROR: realloc failure in hashBody(), body:%s not included\n\n", name); return(-1); }

  sumAsci->bindx[sum][sumAsci->anzb[sum]] = nr;
  sumAsci->anzb[sum]++;
  return(sum);
}


int  gbod( char *name, char *blend, int anz_c, char *cori, char *edge )
{
  static int nr, i, snr;
  int sum=0;

  if(anz_c > 7) return(-1);

  nr=getBodyNr(name);
  if (nr==-1)        /* new body */
  {
    if ((body = (Gbod *)realloc( (Gbod *)body, (anzGeo->b+1)*sizeof(Gbod)) ) == NULL )
     { printf("\n\nERROR: realloc failure in body, body:%s not installed\n\n", name); return(-1); }
    nr=anzGeo->b;
    if ((body[nr].o = (char *)malloc( (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: malloc failure in body, body.o:%s not installed\n\n", name); return(-1); }
    if ((body[nr].s = (int *)malloc( (anz_c+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: malloc failure in body, body.l:%s not installed\n\n", name); return(-1); }
    anzGeo->b++;
    i=strlen(name);
    if((body[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(body[nr].name, name);
    sum=hashBody( sumAsci, name, nr );
    body[nr].nn=-1;
    body[nr].ne=-1;
    body[nr].nod= NULL;
    body[nr].elem= NULL;
  }
  else if (nr<-1)    /* replace a deleted body */
  {
    nr=-(nr+10);
    if ((body[nr].o = (char *)realloc( (char *)body[nr].o, (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: realloc failure in body, body.o:%s not installed\n\n", name); return(-1); }
    if ((body[nr].s = (int *)realloc( (int *)body[nr].s, (anz_c+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in body, body.s:%s not installed\n\n", name); return(-1); }
    i=strlen(name);
    if((body[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(body[nr].name, name);
    sum=hashBody( sumAsci, name, nr );
  }
  else
  {
    if(printFlag) printf (" body in use: Definition will be changed\n");
    if ((body[nr].o = (char *)realloc( (char *)body[nr].o, (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: realloc failure in body, body.o:%s not installed\n\n", name); return(-1); }
    if ((body[nr].s = (int *)realloc( (int *)body[nr].s, (anz_c+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in body, body.s:%s not installed\n\n", name); return(-1); }
    free(body[nr].nod);
    free(body[nr].elem);
    body[nr].nod= NULL;
    body[nr].elem= NULL;
    body[nr].nn=-1;
    body[nr].ne=-1;
  }

  /* for the moment all bodys are + oriented */
  for (i=0; i<anz_c; i++)
  {
    snr=getSurfNr((char*)&edge[i*MAX_LINE_LENGTH]);
    if( snr>-1 ) { body[nr].s[i]=snr; }
    else
    {
      errMsg ("ERROR: surf:%s in body:%s are undefined\n", &edge[i*MAX_LINE_LENGTH], name );
      if(printFlag) printf (" delete body:%s \n",  body[nr].name );
      body[nr].name = (char *)NULL ;
      return(-1);
    }
    body[nr].o[i]=cori[i*2];
  }
  body[nr].ori= '+';
  body[nr].ns= anz_c;
  body[nr].etyp=0;
  body[nr].eattr=0;

  /* print the values of the hash table */
  if(sum<0) exit(-1);
  /*
  else printf(" asciSum:%d asci-indx:%d sums in hash-table:%d name:%s index:%d\n"
  , sum, sumAsci->anzb[sum]-1, sumAsci->anzb[sum]
  , body[sumAsci->bindx[sum][sumAsci->anzb[sum]-1]].name
  , sumAsci->bindx[sum][sumAsci->anzb[sum]-1] );  
  */

  /* orient the new body */
  if ( orientBody( nr ) <0 )
  {
      errMsg ("ERROR: orientBody:%s failed\n", name);
      body[nr].name = (char *)NULL ;
      return(-1);
  }
  for (i=0; i<anz->sets; i++)
  {
    if(( set[i].name != (char *)NULL)&&( set[i].flag=='o')) seta( i, "b", nr );
  }
  return(nr);
}


int  gbod_i( char *name, int blend, int anz_c, char *cori, int *edge )
{
  static int nr, i;
  int sum=0;

  if(anz_c > 7) return(-1);

  nr=getBodyNr(name);
  if (nr==-1)        /* new body */
  {
    if ((body = (Gbod *)realloc( (Gbod *)body, (anzGeo->b+1)*sizeof(Gbod)) ) == NULL )
     { printf("\n\nERROR: realloc failure in body, body:%s not installed\n\n", name); return(-1); }
    nr=anzGeo->b;
    if ((body[nr].o = (char *)malloc( (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: malloc failure in body, body.o:%s not installed\n\n", name); return(-1); }
    if ((body[nr].s = (int *)malloc( (anz_c+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: malloc failure in body, body.l:%s not installed\n\n", name); return(-1); }
    anzGeo->b++;
    i=strlen(name);
    if((body[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(body[nr].name, name);
    sum=hashBody( sumAsci, name, nr );
    body[nr].nn=-1;
    body[nr].ne=-1;
    body[nr].nod= NULL;
    body[nr].elem= NULL;
  }
  else if (nr<-1)    /* replace a deleted body */
  {
    nr=-(nr+10);
    if ((body[nr].o = (char *)realloc( (char *)body[nr].o, (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: realloc failure in body, body.o:%s not installed\n\n", name); return(-1); }
    if ((body[nr].s = (int *)realloc( (int *)body[nr].s, (anz_c+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in body, body.s:%s not installed\n\n", name); return(-1); }
    i=strlen(name);
    if((body[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
    { printf("ERROR: malloc failed\n\n" ); return(-1); }
    strcpy(body[nr].name, name);
    sum=hashBody( sumAsci, name, nr );
  }
  else
  {
    if(printFlag) printf (" body in use: Definition will be changed\n");
    if ((body[nr].o = (char *)realloc( (char *)body[nr].o, (anz_c+1)*sizeof(char)) ) == NULL )
     { printf("\n\nERROR: realloc failure in body, body.o:%s not installed\n\n", name); return(-1); }
    if ((body[nr].s = (int *)realloc( (int *)body[nr].s, (anz_c+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in body, body.s:%s not installed\n\n", name); return(-1); }
    free(body[nr].nod);
    free(body[nr].elem);
    body[nr].nod= NULL;
    body[nr].elem= NULL;
    body[nr].nn=-1;
    body[nr].ne=-1;
  }

  /* for the moment all bodys are + oriented */
  for (i=0; i<anz_c; i++)
  {
    body[nr].s[i]=edge[i];
    body[nr].o[i]=cori[i];
  }
  body[nr].ori= '+';
  body[nr].ns= anz_c;
  body[nr].etyp=0;
  body[nr].eattr=0;

  /* print the values of the hash table */
  if(sum<0) exit(-1);
  /*
  else printf(" asciSum:%d asci-indx:%d sums in hash-table:%d name:%s index:%d\n"
  , sum, sumAsci->anzb[sum]-1, sumAsci->anzb[sum]
  , body[sumAsci->bindx[sum][sumAsci->anzb[sum]-1]].name
  , sumAsci->bindx[sum][sumAsci->anzb[sum]-1] );  
  */

  /* orient the new body */
  if ( orientBody( nr ) <0 )
  {
      errMsg ("ERROR: orientBody:%s failed\n", name);
      body[nr].name = (char *)NULL ;
      return(-1);
  }
  for (i=0; i<anz->sets; i++)
  {
    if(( set[i].name != (char *)NULL)&&( set[i].flag=='o')) seta( i, "b", nr );
  }
  return(nr);
}


void pre_gbod( char *record, int addFlag)
{
  int  length, anz_c, i;
  static char name[MAX_LINE_LENGTH], blend[MAX_LINE_LENGTH], cori[7][2], edge[7][MAX_LINE_LENGTH];

  if (!addFlag) length = sscanf( record, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", name, blend, cori[0], edge[0], cori[1], edge[1], cori[2], edge[2], cori[3], edge[3], cori[4], edge[4], cori[5], edge[5], cori[6], edge[6] );
  else 
  {
    name[0]='!';
    for(i=0; i<7; i++) edge[i][0]='%';
    length = sscanf( record, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s", &name[1], blend, cori[0], &edge[0][1], cori[1], &edge[1][1], cori[2], &edge[2][1], cori[3], &edge[3][1], cori[4], &edge[4][1], cori[5], &edge[5][1], cori[6], &edge[6][1] );
  }

  operateAlias( name, "b " );

  anz_c=(length-2)/2;

  if(printFlag) printf("Gbod:%s ", name );
  if ((compare( blend, "NORM", 4) ==4 )||(compare( blend, "norm", 4) ==4 ))
  {
    if(printFlag) printf("%s ",  blend);
  }
  else
  {
    printf("WARNING: Orientation:%s of Body:%s not recognized and set to NORM\n", blend, name );
    strcpy( blend, "NORM");
  }

  for (i=0; i<anz_c; i++)
  {
    operateAlias( edge[i], "s " );
    if ((compare( cori[i], "+", 1) ==1 )||(compare( cori[i], "-", 1) ==1 ))
    {
      if(printFlag) printf("%s %s ", cori[i], edge[i]);
    }
    else
    {
      printf("ERROR: Orientation:%s of body:%s not recognized\n", cori[i], edge[i] );
      return;
    }
  }
  if(printFlag) printf("\n");

  i=gbod( name, blend, anz_c, &cori[0][0], &edge[0][0] );
  if( i <0)
    printf("ERROR: body could not be created\n");
}


void pre_body( char *record)
{
  int  length, anz_c, i;
  static char name[MAX_LINE_LENGTH], blend[MAX_LINE_LENGTH], cori[7][2], edge[7][MAX_LINE_LENGTH], dummy[MAX_LINE_LENGTH];

  length = sscanf( record, "%s%s%s%s%s%s%s%s%s",
    name, edge[0], edge[1], edge[2], edge[3], edge[4], edge[5], edge[6], dummy);
  operateAlias( name, "b " );

  anz_c=length-1;

  if((anz_c>4)&&(anz_c<8)) 
  {
    strcpy( blend, "NORM"); 
    for (i=0; i<anz_c; i++)
    {
      operateAlias( edge[i], "s " );
      cori[i][0]= '+';
    }
    i=gbod( name, blend, anz_c, &cori[0][0], &edge[0][0] );
    if( i <0)
      printf("ERROR: body could not be created\n");
  }
  else if(anz_c==2) 
  {
    operateAlias( edge[0], "s " );
    operateAlias( edge[1], "s " );
    i=body_( name, &edge[0][0] );
    if( i <0)
      printf("ERROR: body could not be created\n");
  }
  else 
  {
    printf("ERROR: body:%s has %d surfs defined but must have 2 or 5 to 7\n", name, anz_c );
    return;
  }
}


/*------------------------------------------------------------------*/
/* define Nurbs line                                                */
/* returns index of related spline and not of the nurbs-line        */
/*------------------------------------------------------------------*/

int nurl( char *string, int addFlag )
{
  char action[MAX_LINE_LENGTH], name[MAX_LINE_LENGTH], buffer[MAX_LINE_LENGTH], pstart[MAX_LINE_LENGTH], pend[MAX_LINE_LENGTH], dummy[MAX_LINE_LENGTH];
  double  value, x,y,z,weight;
  int    i, j, length, pnr, nr, snr=-1, flag;
  static int pnr1, pnr2;
#if NO_NURL
  int zapset;
#endif
  static int compactFlag;
  static int    div=0;
  double bias=1.;
  
  Rsort *rsort=NULL;
  int ptr[1];

  if (!addFlag) sscanf (string,"%s%s%s", name, action, dummy);
  else
  {
    sscanf (string,"%s%s%s", &name[1], action, dummy);
    if(( compare( action, "DEFI", 4) == 4 )||( compare( action, "defi", 4) == 4 )) name[0]='!';
    else name[0]='%';
  }
  operateAlias( name, "L" );
  if(printFlag) printf (" nurl:%s action:%s \n ",  name, action);

  /* if(compare(name,"L154",4)!=4) return(-1); */

  nr=getNurlNr(name);

  if(( compare( action, "DEFI", 4) == 4 ) || ( compare( action, "defi", 4) == 4 ))
  {
    /* create new NURBS line */
    div=0;

#if NO_NURL
    /* open a set for the controll-points, they will be deleted after comletition */
    if( (zapset=pre_seta( specialset->zap, "i", 0)) <0 ) return(-1);
#endif

    if (nr==-1)        /* new Nurl */
    {
      nr=anzGeo->nurl;
      anzGeo->nurl++;
      if(printFlag) printf ("add NURL Nr:%d Name:%s\n", anzGeo->nurl, name);

      if ((nurbl = (Nurbl *)realloc( (Nurbl *)nurbl, (anzGeo->nurl+1)*sizeof(Nurbl)) ) == NULL )
       { printf("\n\nERROR: realloc failure in Nurl, nurbl:%s not installed\n\n", name); return(-1); }

      i=strlen(name);
      if((nurbl[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
      { printf("ERROR: malloc failed\n\n" ); return(-1); }
      strcpy(nurbl[nr].name, name);
    }
    else if (nr<-1)    /* replace a deleted nurl */
    {
      nr=-(nr+10);
      if(printFlag) printf ("redefine NURL Nr:%d Name:%s\n", anzGeo->nurl, name);
      i=strlen(name);
      if((nurbl[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
      { printf("ERROR: malloc failed\n\n" ); return(-1); }
      strcpy(nurbl[nr].name, name);
      free( nurbl[nr].uknt );
      free( nurbl[nr].ctlpnt );
      free( nurbl[nr].weight );
    }
    else
    {
      if(printFlag) printf ("replace NURL Nr:%d Name:%s\n", anzGeo->nurl, name);
      free( nurbl[nr].uknt );
      free( nurbl[nr].ctlpnt );
      free( nurbl[nr].weight );
    }

    /* define NURBS as incomplete */
    nurbl[nr].endFlag=0;

    /* read the header data of the nurbs with the COMPACT fam statement */
    if(compare(dummy,"COMP",4)==4)
    {
      compactFlag=1;
      if (!addFlag) sscanf (string,"%*s%*s%*s%s%s%d%d%d%d", pstart, pend, &nurbl[nr].u_exp, &nurbl[nr].u_npnt, &nurbl[nr].u_nknt, &div);
      else
      {
        pstart[0]=pend[0]='%';
        sscanf (string,"%*s%*s%*s%s%s%d%d%d%d", &pstart[1], &pend[1], &nurbl[nr].u_exp, &nurbl[nr].u_npnt, &nurbl[nr].u_nknt, &div);
      }
    }
    else if(compare(dummy,"FULL",4)==4)
    {
      compactFlag=0;
      if (!addFlag) sscanf (string,"%*s%*s%*s%s%s%d%d%d%d", pstart, pend, &nurbl[nr].u_exp, &nurbl[nr].u_npnt, &nurbl[nr].u_nknt, &div);
      else
      {
        pstart[0]=pend[0]='%';
        sscanf (string,"%*s%*s%*s%s%s%d%d%d%d", &pstart[1], &pend[1], &nurbl[nr].u_exp, &nurbl[nr].u_npnt, &nurbl[nr].u_nknt, &div);
      }
    }
    else
    {
      compactFlag=0;
      if (!addFlag) sscanf (string,"%*s%*s%s%s%d%d%d%d", pstart, pend, &nurbl[nr].u_exp, &nurbl[nr].u_npnt, &nurbl[nr].u_nknt, &div);
      else
      {
        pstart[0]=pend[0]='%';
        sscanf (string,"%*s%*s%s%s%d%d%d%d", &pstart[1], &pend[1], &nurbl[nr].u_exp, &nurbl[nr].u_npnt, &nurbl[nr].u_nknt, &div);
      }
    }
    operateAlias( pstart, "p" );
    operateAlias( pend, "p" );
    if (nurbl[nr].u_npnt<1)
    {
      errMsg("ERROR: Nurbs[%d]:%s has only %d u_npnts\n\n", nr, nurbl[nr].name, nurbl[nr].u_npnt);
      anzGeo->nurl--;
      nurbl[nr].name = (char *)NULL;
      return(-1);
    }
    if (nurbl[nr].u_nknt<1)
    {
      errMsg("ERROR: Nurbs[%d]:%s has only %d u_nknt\n\n", nr, nurbl[nr].name, nurbl[nr].u_nknt);
      anzGeo->nurl--;
      nurbl[nr].name = (char *)NULL;
      return(-1);
    }

    /* check if points exists */
    pnr1=getPntNr(pstart);
    if (pnr1==-1)        /* new point */
    {
      printf ("ERROR: point %s of Line %s not defined\n", pstart, name);
      anzGeo->nurl--;
      nurbl[nr].name = (char *)NULL;
      return(-1);
    }
    pnr2=getPntNr(pend);
    if (pnr2==-1)        /* new point */
    {
      printf ("ERROR: point %s of Line %s not defined\n", pend, name);
      anzGeo->nurl--;
      nurbl[nr].name = (char *)NULL;
      return(-1);
    }

    /* allocate Knots and control-points */
    if ( (nurbl[nr].uknt =
      (GLfloat *)malloc(  (nurbl[nr].u_nknt+1) * sizeof(GLfloat))) == NULL )
      printf("\n\n ERROR: malloc failed uknt\n\n");
    if ( (nurbl[nr].ctlpnt =
      (GLint *)malloc(  (nurbl[nr].u_npnt+1) * sizeof(GLint))) == NULL )
      printf("\n\n ERROR: malloc failed ctlpnt\n\n");
    if ( (nurbl[nr].weight =
      (GLfloat *)malloc(  (nurbl[nr].u_npnt+1) * sizeof(GLfloat))) == NULL )
      printf("\n\n ERROR: malloc failed ctlpnt\n\n");
  }
  else if(( compare( action, "CONT", 4) == 4 ) || ( compare( action, "cont", 4) == 4 ))
  {
    /* Add a control point */
    if (nr<0)
    {
      printf(" ERROR, NURL:%s not defined\n", name );
      return(-1);
    }
    if(!compactFlag)
    {
      if (!addFlag) length=sscanf (string,"%*s%*s%d%s%lf", &i, buffer, &weight);
      else
      {
        buffer[0]='%';
        length=sscanf (string,"%*s%*s%d%s%lf", &i, &buffer[1], &weight);
      }
      operateAlias( buffer, "p" );
      pnr=getPntNr(buffer);
      if (pnr<0)
      {
        printf("ERROR: Control-Point:%s in Nurbl:%s is not defined, Nurbl will be deleted\n", buffer, nurbl[nr].name );
        anzGeo->nurl--;
        nurbl[nr].name = (char *)NULL;
        return(-1);
      }
      i--;
      if((i<0)&&(i>nurbl[nr].u_nknt)) { printf("ERROR in nurl\n"); exit(1); }
      nurbl[nr].ctlpnt[i] = pnr;
      if(length==3) nurbl[nr].weight[i] =(GLfloat)weight;
      else nurbl[nr].weight[i]=1.;
    }
    else
    {
      length=sscanf (string,"%*s%*s%d%lf%lf%lf%lf", &i, &x,&y,&z, &weight);
      getNewName( buffer, "p" );
      pnr= pnt( buffer, x, y, z, 1);
      i--;
      if((i<0)&&(i>nurbl[nr].u_nknt)) { printf("ERROR in nurl\n"); exit(1); }
      nurbl[nr].ctlpnt[i] = pnr; 
      if(length==5) nurbl[nr].weight[i] =(GLfloat)weight;
      else nurbl[nr].weight[i]=1.;
    }
#if NO_NURL
    /* store the controll-point in the zap-set */
    if((pnr!=pnr1)&&(pnr!=pnr2)) seta( zapset, "p", pnr );
#endif
  }
  else if(( compare( action, "KNOT", 4) == 4 ) || ( compare( action, "knot", 4) == 4 ))
  {
    /* Add a Knot */
    if (nr<0)
    {
      printf(" ERROR, NURL:%s not defined\n", name );
      return(-1);
    }
    sscanf (string,"%*s%*s%d%lf", &i, &value);
    i--;
    if((i<0)&&(i>nurbl[nr].u_nknt)) { printf("ERROR in nurl\n"); exit(1); }

    /* quick fix for bad defined knot sequences. nurl can not be displayed but a line can be derived  */
    if (value<0.) value=0.;
    nurbl[nr].uknt[i] = (GLfloat)value;
  }
  else if(( compare( action, "END", 3) == 3 ) || ( compare( action, "end", 3) == 3 ))
  {
    /* close the NURBS */
    if (nr<0)
    {
      printf(" ERROR, NURL:%s not defined\n", name );
      return(-1);
    }
    nurbl[nr].endFlag=1;       /* define NURBL as complete */
    nurbl[nr].type=GL_MAP1_VERTEX_4;       
    nurbl[nr].u_stride=4;
    nurbl[nr].ctlarray=NULL;


    /* create a spline from the nurbl */
    /* the nurbl will be evaluated at several values of u and points will be created and used */
    /* as spline-points */
    /* create the basic line */
    if(div==0) div=MAX_LINE_DIV;
    pnr= nurbl2seq(nr, nurbl);

    /* search close points in set pnr to p1 and p2 and replace them by p1 and p2 */
    /* calculate all dr between p1 and pnt and sort the indexes according to dr */ 
    if ( (rsort = (Rsort *)malloc( (set[pnr].anz_p+1) * sizeof(Rsort))) == NULL )
      printf("ERROR: realloc failed: Rsort\n\n" ); 
    for (i=0; i<set[pnr].anz_p; i++)
    {
      x=point[pnr1].px-point[set[pnr].pnt[i]].px;
      y=point[pnr1].py-point[set[pnr].pnt[i]].py;
      z=point[pnr1].pz-point[set[pnr].pnt[i]].pz;
      rsort[i].r=sqrt(x*x+y*y+z*z);
      rsort[i].i=i;
    }
    qsort( rsort, set[pnr].anz_p, sizeof(Rsort), (void *)compareRsort );
#if TEST
    for (i=0; i<set[pnr].anz_p; i++)
      printf("%d p:%d r:%lf\n", i, rsort[i].i, rsort[i].r); 
#endif
    // printf("type:%d name:%s p1[%d]:%s p2[%d]:%s p[%d]:%s\n", set[pnr].type, set[pnr].name, pnr2, point[pnr2].name, pnr1, point[pnr1].name, set[pnr].pnt[rsort[0].i], point[set[pnr].pnt[rsort[0].i]].name);
    if(pnr1!=set[pnr].pnt[rsort[0].i])
    {
      ptr[0]=set[pnr].pnt[rsort[0].i];
      // printf("type:%d name:%s p1[%d]:%s p2[%d]:%s p[%d]:%s\n", set[pnr].type, set[pnr].name, pnr2, point[pnr2].name, pnr1, point[pnr1].name, ptr[0], point[ptr[0]].name);
      set[pnr].pnt[rsort[0].i]=pnr1;
      //delPnt( 1, ptr );  /* do not delete. endpoints from set pnt might be the same as the nurl-endpoints */
    }

    /* calculate all dr between p2 and pnt and sort the indexes according to dr */ 
    for (i=0; i<set[pnr].anz_p; i++)
    {
      x=point[pnr2].px-point[set[pnr].pnt[i]].px;
      y=point[pnr2].py-point[set[pnr].pnt[i]].py;
      z=point[pnr2].pz-point[set[pnr].pnt[i]].pz;
      rsort[i].r=sqrt(x*x+y*y+z*z);
      rsort[i].i=i;
    }
    qsort( rsort, set[pnr].anz_p, sizeof(Rsort), (void *)compareRsort );
    if(pnr2!=set[pnr].pnt[rsort[0].i])
    {
      ptr[0]=set[pnr].pnt[rsort[0].i];
      set[pnr].pnt[rsort[0].i]=pnr2;
      //delPnt( 1, ptr );  /* do not delete. endpoints from set pnt might be the same as the nurl-endpoints */
    }

    /* keep only points between p1 and p2 */
    flag=j=0;
    for (i=0; i<set[pnr].anz_p; i++)
    {
      if((set[pnr].pnt[i]==pnr1) || (set[pnr].pnt[i]==pnr2)) { rsort[j].i=set[pnr].pnt[i]; j++; flag=!flag; }
      else if(flag) { rsort[j].i=set[pnr].pnt[i]; j++; }
    }
    set[pnr].anz_p=j;
    for (i=0; i<set[pnr].anz_p; i++) set[pnr].pnt[i]=rsort[i].i;
    free(rsort);

    /* determine bias and division */
    splitBiasDiv(&div, &bias);
    snr=line_i( name, pnr1, pnr2, pnr, div, bias, 's' );

#if NO_NURL
    /* for the moment delete the nurl and the controll-points */
    nurbl[nr].name = (char *)NULL;
#else
    for (i=0; i<anz->sets; i++)
    {
      if ( set[i].flag=='o') seta( i, "L", nr );
    }
    nurbl[nr].Nurb = (GLUnurbsObj *)gluNewNurbsRenderer();
#endif
  }
  else
  {
    printf("ERROR: %s not known\n", action);
  }
  return(snr);
}


/*------------------------------------------------------------------*/
/* define Nurbs surface                                             */
/*------------------------------------------------------------------*/
void delNurs( int anzs, int *number )
{
  int j, k, p, *nrbuffer;

  delNursFlag=1;
/*
  printf ("sum:%d num:%d surf:%s\n", anzs, number[0], surf[number[0]].name );
*/
  if ((nrbuffer = (int *)malloc((anzs+1)*sizeof(int)) ) == NULL )
     { printf("\n\nERROR: realloc failure in delBody\n\n"); return; }

  /* nessesary to store the numbers in a independent area  */
  for (j=0; j<anzs; j++)
    nrbuffer[j]=number[j];

  for (j=0; j<anzs; j++) if( nurbs[nrbuffer[j]].name != (char *)NULL )
  {
    for (k=0; k<anz->sets; k++)
    {
      if(set[k].type==0)
       if( set[k].name != (char *)NULL )
        setr( k, "S", nrbuffer[j]);
    }
    for (k=0; k<anzGeo->sh; k++)
    {
      if((shape[k].type==4)&&(shape[k].p[0]==nrbuffer[j])) delShape(1, &k);
    }
    if(printFlag) printf (" delete nurs:%s \n",  nurbs[nrbuffer[j]].name );
    free(nurbs[nrbuffer[j]].name);
    nurbs[nrbuffer[j]].name = (char *)NULL ;
    free(nurbs[nrbuffer[j]].uknt);
    free(nurbs[nrbuffer[j]].vknt);
    free(nurbs[nrbuffer[j]].ctlarray);
    nurbs[nrbuffer[j]].uknt=NULL;
    nurbs[nrbuffer[j]].vknt=NULL;
    nurbs[nrbuffer[j]].ctlarray=NULL;

    for(p=0; p<nurbs[nrbuffer[j]].patches; p++)
    {
      for(k=0; k<nurbs[nrbuffer[j]].nc[p]; k++)
      {  free(nurbs[nrbuffer[j]].uv[p][k]);
         free(nurbs[nrbuffer[j]].xyz[p][k]); }
      free(nurbs[nrbuffer[j]].uv[p]);
      free(nurbs[nrbuffer[j]].xyz[p]);
      free(nurbs[nrbuffer[j]].np[p]);
    }
    free(nurbs[nrbuffer[j]].uv);
    free(nurbs[nrbuffer[j]].xyz);
    free(nurbs[nrbuffer[j]].np);
    free(nurbs[nrbuffer[j]].umax);
    free(nurbs[nrbuffer[j]].vmax);
    nurbs[nrbuffer[j]].uv=NULL;
    nurbs[nrbuffer[j]].xyz=NULL;
    nurbs[nrbuffer[j]].np=NULL;
    nurbs[nrbuffer[j]].umax=NULL;
    nurbs[nrbuffer[j]].vmax=NULL;

    for(k=0; k<nurbs[nrbuffer[j]].u_npnt; k++)
    {  free(nurbs[nrbuffer[j]].ctlpnt[k]);
       free(nurbs[nrbuffer[j]].weight[k]); }
    free(nurbs[nrbuffer[j]].ctlpnt);
    free(nurbs[nrbuffer[j]].weight);
    nurbs[nrbuffer[j]].ctlpnt=NULL;
    nurbs[nrbuffer[j]].weight=NULL;

    gluDeleteNurbsRenderer(nurbs[nrbuffer[j]].Nurb);
    nurbs[nrbuffer[j]].Nurb=NULL;
  }
  free(nrbuffer);
}


int hashNurs( SumAsci *sumAsci, char *name, int nr)
{
  int i=0,j=0, n;
  int sum=0;

  while(name[i]!='\0') { sum+=name[i]*(++j); i++; }

  /* check if sum is higher as the allocated value */
  /* else look for a free entry */
  if(sum>sumAsci->max_sumS)
  {
    if ((sumAsci->anzS=(int *)realloc( (int *)sumAsci->anzS, (sum+1)*sizeof(int)) ) == NULL )
    { printf("\n\nERROR: realloc failure in hashNurs(), Nurs:%s not included\n\n", name); return(-1); }
    if ((sumAsci->Sindx=(int **)realloc( (int **)sumAsci->Sindx, (sum+1)*sizeof(int *)) ) == NULL )
    { printf("\n\nERROR: realloc failure in hashNurs(), Nurs:%s not included\n\n", name); return(-1); }
    for(i=sumAsci->max_sumS+1; i<=sum; i++) { sumAsci->anzS[i]=0; sumAsci->Sindx[i]=NULL; }
    sumAsci->max_sumS=sum;
  }
  else
  {
    if (delNursFlag)
    for (i=0; i<sumAsci->anzS[sum]; i++) 
    {
      n=sumAsci->Sindx[sum][i];
      if( nurbs[n].name == (char *)NULL )
      {
        /* already existing space to fill */
        sumAsci->Sindx[sum][i]=nr;
        return(sum);
      }
    }
  }

  /* alloc of a new entry in the hash table */
  if ((sumAsci->Sindx[sum] 
  =(int *)realloc( (int *)sumAsci->Sindx[sum], (sumAsci->anzS[sum]+1)*sizeof(int)) ) == NULL )
  { printf("\n\nERROR: realloc failure in hashNurs(), nurs:%s not included\n\n", name); return(-1); }

  sumAsci->Sindx[sum][sumAsci->anzS[sum]] = nr;
  sumAsci->anzS[sum]++;
  return(sum);
}


int nurs( char *string, int addFlag )
{
  char action[MAX_LINE_LENGTH], name[MAX_LINE_LENGTH], buffer[MAX_LINE_LENGTH], dummy[MAX_LINE_LENGTH];
  double  x,y,z, weight, value;
  int    i, j, p, k, pnr, length, nr;
  int    zapset;
  int    sum=0;
  static int    switchFlag=0; /* for IRIX: if u_exp ==1 switch u,v */
  static int    compactFlag;

  if (!addFlag) sscanf (string,"%s%s%s", name, action, dummy);
  else
  {
    sscanf (string,"%s%s%s", &name[1], action, dummy);
    if(( compare( action, "DEFI", 4) == 4 )||( compare( action, "defi", 4) == 4 )) name[0]='!';
    else name[0]='%';
  }
  operateAlias( name, "S" );
  if(printFlag) printf (" nurs:%s action:%s \n ",  name, action);

  nr=getNursNr(name);

  if(( compare( action, "DEFI", 4) == 4 ) || ( compare( action, "defi", 4) == 4 ))
  {
    /* create new NURBS shape */

#if NO_NURS
    /* open a set for the controll-points, they will be deleted after comletition */
    if( (zapset=pre_seta( specialset->zap, "i", 0)) <0 ) return(-1);
#endif

    if (nr==-1)        /* new nurs */
    {
      if(printFlag) printf ("store NURS Nr:%d Name:%s\n", anzGeo->nurs+1, name);

      if ((nurbs = (Nurbs *)realloc( (Nurbs *)nurbs, (anzGeo->nurs+1)*sizeof(Nurbs)) ) == NULL )
      { printf("\n\nERROR: realloc failure in Nurs, nurbs:%s not installed\n\n", name); return(-1); }

      nr=anzGeo->nurs;
      sum=hashNurs( sumAsci, name, nr );
      anzGeo->nurs++;
      i=strlen(name);
      if((nurbs[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
      { printf("ERROR: malloc failed\n\n" ); return(-1); }
      strcpy(nurbs[nr].name, name);
    }
    else if (nr<-1)    /* replace a deleted nurs */
    {
      nr=-(nr+10);

      if ((nurbs = (Nurbs *)realloc( (Nurbs *)nurbs, (anzGeo->nurs+1)*sizeof(Nurbs)) ) == NULL )
      { printf("\n\nERROR: realloc failure in Nurs, nurbs:%s not installed\n\n", name); return(-1); }

      i=strlen(name);
      if((nurbs[nr].name= (char *)malloc((i+1)*sizeof(char))) == NULL )
      { printf("ERROR: malloc failed\n\n" ); return(-1); }
      strcpy(nurbs[nr].name, name);
      free( nurbs[nr].uknt );
      free( nurbs[nr].vknt );
      for (i=0; i<nurbs[nr].u_npnt; i++)
      {
        free( nurbs[nr].ctlpnt[i] );
      }
      free( nurbs[nr].ctlpnt );
      free( nurbs[nr].weight );

      for(p=0; p<nurbs[nr].patches; p++)
      {
        for(k=0; k<nurbs[nr].nc[p]; k++)
        {  free(nurbs[nr].uv[p][k]);
           free(nurbs[nr].xyz[p][k]); }
        free(nurbs[nr].uv[p]);
        free(nurbs[nr].xyz[p]);
        free(nurbs[nr].np[p]);
      }
      free(nurbs[nr].uv);
      free(nurbs[nr].xyz);
      free(nurbs[nr].np);
      free(nurbs[nr].nc);
      free(nurbs[nr].umax);
      free(nurbs[nr].vmax);
      nurbs[nr].uv=NULL;
      nurbs[nr].xyz=NULL;
      nurbs[nr].np=NULL;
      nurbs[nr].nc=NULL;
      nurbs[nr].umax=NULL;
      nurbs[nr].vmax=NULL;

      sum=hashNurs( sumAsci, name, nr );
    }
    else
    {
      if(printFlag) printf ("redefine NURS:%s\n", name);
      free( nurbs[nr].uknt );
      free( nurbs[nr].vknt );
      for (i=0; i<nurbs[nr].u_npnt; i++)
      {
        free( nurbs[nr].ctlpnt[i] );
        free( nurbs[nr].weight[i] );
      }
      free( nurbs[nr].ctlpnt );
      free( nurbs[nr].weight );

      for(p=0; p<nurbs[nr].patches; p++)
      {
        for(k=0; k<nurbs[nr].nc[p]; k++)
        {  free(nurbs[nr].uv[p][k]);
           free(nurbs[nr].xyz[p][k]); }
        free(nurbs[nr].uv[p]);
        free(nurbs[nr].xyz[p]);
        free(nurbs[nr].np[p]);
      }
      free(nurbs[nr].uv);
      free(nurbs[nr].xyz);
      free(nurbs[nr].np);
      free(nurbs[nr].nc);
      free(nurbs[nr].umax);
      free(nurbs[nr].vmax);
      nurbs[nr].uv=NULL;
      nurbs[nr].xyz=NULL;
      nurbs[nr].np=NULL;
      nurbs[nr].nc=NULL;
      nurbs[nr].umax=NULL;
      nurbs[nr].vmax=NULL;
    }

    /* define NURBS as incomplete */
    nurbs[nr].endFlag=0;

    /* read the header data of the nurbs with the COMPACT fam statement */
    if(compare(dummy,"COMP",4)==4)
    {
      compactFlag=1;
      sscanf (string,"%*s%*s%*s%d%d%d%d%d%d", &nurbs[nr].u_exp, &nurbs[nr].v_exp, &nurbs[nr].u_npnt, &nurbs[nr].v_npnt, &nurbs[nr].u_nknt, &nurbs[nr].v_nknt);
    }
    /* read the header data of the nurbs with the FULL fam statement */
    else if(compare(dummy,"FULL",4)==4)
    {
      compactFlag=0;
      sscanf (string,"%*s%*s%*s%d%d%d%d%d%d", &nurbs[nr].u_exp, &nurbs[nr].v_exp, &nurbs[nr].u_npnt, &nurbs[nr].v_npnt, &nurbs[nr].u_nknt, &nurbs[nr].v_nknt);
    }
    else
    {
      compactFlag=0;
      sscanf (string,"%*s%*s%d%d%d%d%d%d", &nurbs[nr].u_exp, &nurbs[nr].v_exp, &nurbs[nr].u_npnt, &nurbs[nr].v_npnt, &nurbs[nr].u_nknt, &nurbs[nr].v_nknt);
    }

    /* if its an irix-computer handle a known bug in the libGL.so by switching u and v */
    switchFlag=0;
    if( (compare(cursys->sysname,"IRIX",4)==4)&&(nurbs[nr].u_exp==1))
    {
      switchFlag=1;
      p=nurbs[nr].u_exp;
      nurbs[nr].u_exp=nurbs[nr].v_exp;
      nurbs[nr].v_exp=p;

      p=nurbs[nr].u_npnt;
      nurbs[nr].u_npnt=nurbs[nr].v_npnt;
      nurbs[nr].v_npnt=p;

      p=nurbs[nr].u_nknt;
      nurbs[nr].u_nknt=nurbs[nr].v_nknt;
      nurbs[nr].v_nknt=p;
    }

    if (nurbs[nr].u_npnt<1)
    {
      errMsg("ERROR: Nurbs[%d]:%s has only %d u_npnts\n\n",
         nr, nurbs[nr].name, nurbs[nr].u_npnt);
      anzGeo->nurs--;
      nurbs[nr].name = (char *)NULL;
      return(-1);
    }
    if (nurbs[nr].v_npnt<1)
    {
      errMsg("ERROR: Nurbs[%d]:%s has only %d v_npnts\n\n",
         nr, nurbs[nr].name, nurbs[nr].u_npnt);
      anzGeo->nurs--;
      nurbs[nr].name = (char *)NULL;
      return(-1);
    }

    /* allocate Knots and control-points */
    if ( (nurbs[nr].uknt =
      (float *)malloc(  (nurbs[nr].u_nknt+1) * sizeof(float))) == NULL )
      printf("\n\n ERROR: malloc failed uknt\n\n");
    if ( (nurbs[nr].vknt =
      (float *)malloc(  (nurbs[nr].v_nknt+1) * sizeof(float))) == NULL )
      printf("\n\n ERROR: malloc failed vknt\n\n");

    if ( (nurbs[nr].ctlpnt =
      (int **)malloc(  (nurbs[nr].u_npnt+1) * sizeof(int *))) == NULL )
      printf("\n\n ERROR: malloc failed ctlpnt\n\n");
    for (i=0; i<nurbs[nr].u_npnt; i++)
    {
      if ( (nurbs[nr].ctlpnt[i] =
        (int *)malloc(  (nurbs[nr].v_npnt+1) * sizeof( int ))) == NULL )
        printf("\n\n ERROR: malloc failed ctlpnt[i]\n\n");
    }

    if ( (nurbs[nr].weight =
      (float **)malloc(  (nurbs[nr].u_npnt+1) * sizeof(float *))) == NULL )
      printf("\n\n ERROR: malloc failed weight\n\n");
    for (i=0; i<nurbs[nr].u_npnt; i++)
    {
      if ( (nurbs[nr].weight[i] =
        (float *)malloc(  (nurbs[nr].v_npnt+1) * sizeof(float))) == NULL )
        printf("\n\n ERROR: malloc failed weight[i]\n\n");
    }
  }
  else if(( compare( action, "CONT", 4) == 4 ) || ( compare( action, "cont", 4) == 4 ))
  {
    /* Add a control point with weight */
    if (nr<0)
    {
      printf(" ERROR, NURS:%s not defined\n", name );
      return(-1);
    }

    /* read the control point by its coordinates (x y z). Used by fam. */
    /* length=sscanf (string,"%*s%*s%d%d%lf%lf%lf%s", &i, &j, &x,&y,&z, dummy); */
    /* read the control point by its name (buffer) */
   if(!compactFlag)
    {
      if (!addFlag) length=sscanf (string,"%*s%*s%d%d%s%lf", &i, &j, buffer, &weight);
      else 
      {
        buffer[0]='%';
        length=sscanf (string,"%*s%*s%d%d%s%lf", &i, &j, &buffer[1], &weight);
      }
      operateAlias( buffer, "p" );
      pnr=getPntNr(buffer);
      if (pnr<0)
      {
        printf("ERROR: Control-Point:%s in Nurbs:%s is not defined, Nurbs will be deleted\n", buffer, nurbs[nr].name );
        anzGeo->nurs--;
        nurbs[nr].name = (char *)NULL;
        return(-1);
      }
      if(switchFlag)
      {
        p=i;
        i=j;
        j=p;
      }
      i--;
      j--;
      nurbs[nr].ctlpnt[i][j] = pnr;
      if(length==4) nurbs[nr].weight[i][j] =weight;
      else nurbs[nr].weight[i][j]=1.;
    }
    else
    {
      length=sscanf (string,"%*s%*s%d%d%lf%lf%lf%lf", &i, &j, &x,&y,&z, &weight);
      if(switchFlag)
      {
        p=i;
        i=j;
        j=p;
      }
      i--;
      j--;
      getNewName( buffer, "p" );
      pnr= pnt( buffer, x, y, z, 0);
      nurbs[nr].ctlpnt[i][j] = pnr;
      if(length==6) nurbs[nr].weight[i][j] =weight;
      else nurbs[nr].weight[i][j]=1.;
    }
 #if NO_NURS
    /* store the controll-point in the zap-set */
    seta( zapset, "p", pnr );
#endif
  }
  else if(( compare( action, "KNOT", 4) == 4 ) || ( compare( action, "knot", 4) == 4 ))
  {
    /* Add a Knot */
    if (nr<0)
    {
      printf(" ERROR, NURS:%s not defined\n", name );
      return(-1);
    }

    /* read the knot, direction (u|v), index, value */
    sscanf (string,"%*s%*s%s%d%lf", buffer, &i, &value);
    if(switchFlag)
    {
      if((buffer[0]=='U')||(buffer[0]=='u')) buffer[0]='V';
      else buffer[0]='U';
    }

    i--;
    if((buffer[0]=='U')||(buffer[0]=='u'))
    {
      if(i<nurbs[nr].u_nknt) nurbs[nr].uknt[i] = value;
      else printf("WARNING: definition of NURS:%s in error\n", nurbs[nr].name);
    }
    else if((buffer[0]=='V')||(buffer[0]=='v'))
    {
      if(i<nurbs[nr].v_nknt) nurbs[nr].vknt[i]=value;
      else printf("WARNING: definition of NURS:%s in error\n", nurbs[nr].name);
    }
    else { printf(" ERROR: parameter:%s not U or V \n", buffer); return(-1); }
  }
  else if(( compare( action, "END", 3) == 3 ) || ( compare( action, "end", 3) == 3 ))
  {
    /* close the NURBS */
    if (nr<0)
    {
      printf(" ERROR, NURS:%s not defined\n", name );
      return(-1);
    }
    nurbs[nr].endFlag=1;       
    nurbs[nr].type=GL_MAP2_VERTEX_4;       
    nurbs[nr].u_stride=4* nurbs[nr].v_npnt;
    nurbs[nr].v_stride=4;
    nurbs[nr].ctlarray=(GLfloat *)NULL;

    /* additional variables for the trimming */
    nurbs[nr].trimFlag=0;
    nurbs[nr].patches=0;
    nurbs[nr].nc=NULL;
    nurbs[nr].uv=NULL;
    nurbs[nr].xyz=NULL;
    nurbs[nr].np=NULL;
    nurbs[nr].umax=NULL;
    nurbs[nr].vmax=NULL;
#if NO_NURS
    /* for the moment delete the nurs and the controll-points */
    nurbs[nr].name = (char *)NULL;
#else
    for (i=0; i<anz->sets; i++)
    {
      if ( set[i].flag=='o') seta( i, "S", nr );
    }
    
    /* first uexp+1 vexp+1 knot-vals must be the same */
    for (i=1; i<=nurbs[nr].u_exp; i++)  if(nurbs[nr].uknt[i]!=nurbs[nr].uknt[0]) printf(" ERROR: Nurbs:%s has a knot-multiplicity(u) of:%d but needs:%d. Correct the Nurbs definition before continuing\n",nurbs[nr].name,i,nurbs[nr].u_exp+1 ); 
    for (i=1; i<=nurbs[nr].v_exp; i++)  if(nurbs[nr].vknt[i]!=nurbs[nr].vknt[0]) printf(" ERROR: Nurbs:%s has a knot-multiplicity(v) of:%d but needs:%d. Correct the Nurbs definition before continuing\n",nurbs[nr].name,i,nurbs[nr].v_exp+1 );  
    for (i=nurbs[nr].u_nknt-nurbs[nr].u_exp; i<nurbs[nr].u_nknt; i++) { if(nurbs[nr].uknt[i]!=nurbs[nr].uknt[nurbs[nr].u_nknt-1-nurbs[nr].u_exp]) printf(" ERROR:  Nurbs:%s has a knot-multiplicity(u) of:%d but needs:%d. Correct the Nurbs definition before continuing\n",nurbs[nr].name,i+1-(nurbs[nr].u_nknt-nurbs[nr].u_exp),nurbs[nr].u_exp+1 ); break; } 
    for (i=nurbs[nr].v_nknt-nurbs[nr].v_exp; i<nurbs[nr].v_nknt; i++) { if(nurbs[nr].vknt[i]!=nurbs[nr].vknt[nurbs[nr].v_nknt-1-nurbs[nr].v_exp]) printf(" ERROR:  Nurbs:%s has a knot-multiplicity(v) of:%d but needs:%d. Correct the Nurbs definition before continuing\n",nurbs[nr].name,i+1-(nurbs[nr].v_nknt-nurbs[nr].v_exp),nurbs[nr].v_exp+1 );  break; } 

    /*Quickfix: negative values alone are set to zero */
    for (i=0; i<nurbs[nr].u_nknt; i++) if(nurbs[nr].uknt[i]<0.)
    { printf(" ERROR: nurbs:%s has negative uknots:%lf -> set to 0.\n", nurbs[nr].name, nurbs[nr].uknt[i]); nurbs[nr].uknt[i]=0.; }
    for (i=0; i<nurbs[nr].v_nknt; i++) if(nurbs[nr].vknt[i]<0.)
    { printf(" ERROR: nurbs:%s has negative vknots:%lf -> set to 0.\n", nurbs[nr].name, nurbs[nr].vknt[i]); nurbs[nr].vknt[i]=0.; }
    /*
    for (i=0; i<nurbs[nr].u_nknt; i++) printf("ku:%lf\n", nurbs[nr].uknt[i]);
    for (i=0; i<nurbs[nr].v_nknt; i++) printf("kv:%lf\n", nurbs[nr].vknt[i]);
    */
    
    nurbs[nr].Nurb = (GLUnurbsObj *)gluNewNurbsRenderer();
    repNurs(nr);

    /* create a shape of the same name for reference in surfaces */
    if( shape_i( nurbs[nr].name, 4, nr, 0, 0) <0)
      printf("ERROR: shape could not be created\n");
#endif
  }
  else
  {
    printf("ERROR: %s not known\n", action);
  }
  return(nr);
}
