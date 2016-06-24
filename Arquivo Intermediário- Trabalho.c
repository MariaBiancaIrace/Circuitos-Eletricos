/*
Programa de demonstracao de analise nodal modificada
Por Antonio Carlos M. de Queiroz acmq@coe.ufrj.br
Versao 1.0 - 6/9/2000
Versao 1.0a - 8/9/2000 Ignora comentarios
Versao 1.0b - 15/9/2000 Aumentado Yn, retirado Js
Versao 1.0c - 19/2/2001 Mais comentarios
Versao 1.0d - 16/2/2003 Tratamento correto de nomes em minusculas
Versao 1.0e - 21/8/2008 Estampas sempre somam. Ignora a primeira linha
Versao 1.0f - 21/6/2009 Corrigidos limites de alocacao de Yn
Versao 1.0g - 15/10/2009 Le as linhas inteiras
Versao 1.0h - 18/6/2011 Estampas correspondendo a modelos
Versao 1.0i - 03/11/2013 Correcoes em *p e saida com sistema singular.
Versao 1.0j - 26/11/2015 Evita operacoes com zero.
Versao 1.0k - 23/06/2016 Calcula P.O. com L, C e K (o acoplamento é ignorado, pois P.O. é análise DC)
Versao 1.0l - 24/06/2016 Leitura do netlist para elemento MOS (ainda falta corrigir algumas coisas)
*/

/*
Elementos aceitos e linhas do netlist:
Resistor:      R<nome> <no+> <no-> <resistencia>
Indutor:       L<nome> <nó+> <nó-> <indutancia>
Acoplamento:   K<nome> <LA> <LB-> <k> (indutores LA e LB já declarados)
Capacitor:     C<nome> <nó+> <nó-> <capacitancia>
VCCS:          G<nome> <io+> <io-> <vi+> <vi-> <transcondutancia>
VCVC:          E<nome> <vo+> <vo-> <vi+> <vi-> <ganho de tensao>
CCCS:          F<nome> <io+> <io-> <ii+> <ii-> <ganho de corrente>
CCVS:          H<nome> <vo+> <vo-> <ii+> <ii-> <transresistencia>
Fonte I:       I<nome> <io+> <io-> <corrente>
Fonte V:       V<nome> <vo+> <vo-> <tensao>
Amp. op.:      O<nome> <vo1> <vo2> <vi1> <vi2>
TransistorMOS: M<nome> <nód> <nóg> <nós> <nób> <NMOS ou PMOS> L=<comprimento> W=<largura> <K> <Vt0> <lambda> <gama> <phi> <Ld>
As fontes F e H tem o ramo de entrada em curto
O amplificador operacional ideal tem a saida suspensa
Os nos podem ser nomes
*/

#define versao "1.0j - 26/11/2015"
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#define MAX_LINHA 80
#define MAX_NOME 11
#define MAX_ELEM 50
#define MAX_NOS 50
#define TOLG 1e-9
#define DEBUG

typedef struct elemento { /* Elemento do netlist */
  char nome[MAX_NOME];
  double valor;
  int a,b,c,d,x,y;
} elemento;

elemento netlist[MAX_ELEM]; /* Netlist */

typedef struct acoplamento {
  char lA[MAX_NOME],lB[MAX_NOME];
} acoplamento;

acoplamento acop_K;

typedef struct transitorMOS {
   char tipo[4];
   double comp,larg,transK,vt0,lambda,gama,phi,ld;
} transistorMOS;

transistorMOS mos;

double ind_L[MAX_ELEM], cap_C[MAX_ELEM]; /*guarda os valores de indutancia e capacitancia p/ serem utilizados no modelo de peq. sinais*/ 

int
  ne, /* Elementos */
  nv, /* Variaveis */
  nn, /* Nos */
  i,j,k,
  inc_L, inc_C,
  ne_extra,nao_linear;

char
/* Foram colocados limites nos formatos de leitura para alguma protecao
   contra excesso de caracteres nestas variaveis */
  nomearquivo[MAX_LINHA+1],
  tipo,
  na[MAX_NOME],nb[MAX_NOME],nc[MAX_NOME],nd[MAX_NOME],
  lista[MAX_NOS+1][MAX_NOME+2], /*Tem que caber jx antes do nome */
  txt[MAX_LINHA+1],
  *p;
FILE *arquivo;

double
  g,
  Yn[MAX_NOS+1][MAX_NOS+2];

/* Resolucao de sistema de equacoes lineares.
   Metodo de Gauss-Jordan com condensacao pivotal */
int resolversistema(void)
{
  int i,j,l,a,inc_L,inc_C;
  double t, p;

  for (i=1; i<=nv; i++) {
    t=0.0;
    a=i;
    for (l=i; l<=nv; l++) {
      if (fabs(Yn[l][i])>fabs(t)) {
	a=l;
	t=Yn[l][i];
      }
    }
    if (i!=a) {
      for (l=1; l<=nv+1; l++) {
	p=Yn[i][l];
	Yn[i][l]=Yn[a][l];
	Yn[a][l]=p;
      }
    }
    if (fabs(t)<TOLG) {
      printf("Sistema singular\n");
      return 1;
    }
    for (j=nv+1; j>0; j--) {  /* Basta j>i em vez de j>0 */
      Yn[i][j]/= t;
      p=Yn[i][j];
      if (p!=0)  /* Evita operacoes com zero */
        for (l=1; l<=nv; l++) {  
	  if (l!=i)
	    Yn[l][j]-=Yn[l][i]*p;
        }
    }
  }
  return 0;
}

/* Rotina que conta os nos e atribui numeros a eles */
int numero(char *nome)
{
  int i,achou;

  i=0; achou=0;
  while (!achou && i<=nv)
    if (!(achou=!strcmp(nome,lista[i]))) i++;
  if (!achou) {
    if (nv==MAX_NOS) {
      printf("O programa so aceita ate %d nos\n",nv);
      exit(1);
    }
    nv++;
    strcpy(lista[nv],nome);
    return nv; /* novo no */
  }
  else {
    return i; /* no ja conhecido */
  }
}

/*void clrscr() {
  #ifdef WINDOWS
  system("cls");
  #endif
  #ifdef LINUX
  system("clear");
  #endif
}*/

int main(void)
{
  //clrscr();
  printf("Programa demonstrativo de analise nodal modificada\n");
  printf("Por Antonio Carlos M. de Queiroz - acmq@coe.ufrj.br\n");
  printf("Versao %s\n",versao);
 denovo:
  /* Leitura do netlist */
  ne=0; nv=0; inc_L=0; inc_C=0; ne_extra=0; nao_linear=0; strcpy(lista[0],"0");
  printf("Nome do arquivo com o netlist (ex: mna.net): ");
  scanf("%50s",nomearquivo);
  arquivo=fopen(nomearquivo,"r");
  if (arquivo==0) {
    printf("Arquivo %s inexistente\n",nomearquivo);
    goto denovo;
  }
  printf("\nAnálise no Ponto de Operação (P.O.)\n\n");
  printf("Lendo netlist:\n");
  fgets(txt,MAX_LINHA,arquivo);
  printf("Titulo: %s",txt);
  while (fgets(txt,MAX_LINHA,arquivo)) { //leitura do netlist linha por linha
    ne++; /* Nao usa o netlist[0] */
    if (ne>MAX_ELEM) {
      printf("O programa so aceita ate %d elementos\n",MAX_ELEM);
      exit(1);
    }
    txt[0]=toupper(txt[0]);
    tipo=txt[0];
    sscanf(txt,"%10s",netlist[ne].nome);
    p=txt+strlen(netlist[ne].nome); /* Inicio dos parametros */
    /* O que e lido depende do tipo */
    if (tipo=='R' || tipo=='L' || tipo=='C' || tipo=='I' || tipo=='V') {
      sscanf(p,"%10s%10s%lg",na,nb,&netlist[ne].valor);
	  if (tipo=='L') {     //substitui a indutancia pela baixa resistencia e armazena a indutancia em outra var
		  inc_L++; 	
		  ind_L[inc_L] = netlist[ne].valor;
		  netlist[ne].valor = 1e-9;
		  printf("%s %s %s %g\n",netlist[ne].nome,na,nb,ind_L[inc_L]);
	  }
	  else if (tipo=='C') {     //substitui a capacitancia pela alta resistencia e armazena a capacitancia em outra var
		  inc_C++;
                  cap_C[inc_C] = netlist[ne].valor;
		  netlist[ne].valor = 1e9;
		  printf("%s %s %s %g\n",netlist[ne].nome,na,nb,cap_C[inc_C]);
	  }
	  else 
	  	printf("%s %s %s %g\n",netlist[ne].nome,na,nb,netlist[ne].valor);	   
      netlist[ne].a=numero(na);
      netlist[ne].b=numero(nb);
	}
	
	else if (tipo=='K') {
		sscanf(p,"%10s%10s%lg",acop_K.lA,acop_K.lB,&netlist[ne].valor);
		printf("%s %s %s %g\n",netlist[ne].nome,acop_K.lA,acop_K.lB,netlist[ne].valor);
	}
	
    else if (tipo=='G' || tipo=='E' || tipo=='F' || tipo=='H') {
      sscanf(p,"%10s%10s%10s%10s%lg",na,nb,nc,nd,&netlist[ne].valor);
      printf("%s %s %s %s %s %g\n",netlist[ne].nome,na,nb,nc,nd,netlist[ne].valor);
      netlist[ne].a=numero(na);
      netlist[ne].b=numero(nb);
      netlist[ne].c=numero(nc);
      netlist[ne].d=numero(nd);
    }
    else if (tipo=='O') {
      sscanf(p,"%10s%10s%10s%10s",na,nb,nc,nd);
      printf("%s %s %s %s %s\n",netlist[ne].nome,na,nb,nc,nd);
      netlist[ne].a=numero(na);
      netlist[ne].b=numero(nb);
      netlist[ne].c=numero(nc);
      netlist[ne].d=numero(nd);
    }
    
    else if (tipo=='M') {
    	nao_linear++;
    	sscanf(p,"%10s%10s%10s%10s%10s%lg%lg%lg%lg%lg%lg%lg%lg",na,nb,nc,nd,mos.tipo,mos.comp,mos.larg,mos.vt0,mos.lambda,mos.gama,mos.phi,mos.ld);
    	printf("%s %s %s %s %s %s %g %g %g %g %g %g %g %g",netlist[ne].nome,na,nb,nc,nd,mos.tipo,mos.comp,mos.larg,mos.vt0,mos.lambda,mos.gama,mos.phi,mos.ld);
    	/*ne_extra: quantidade de nós referentes ao elementos extras do modelo linearizado*/
    	ne_extra=ne;
    	//resistor RDS
    	netlist[ne_extra].nome="RDS"+ne_extra;
    	netlist[ne_extra].a=numero(na);
    	netlist[ne_extra].c=numero(nc);
    	ne_extra++;
    	//fonte de corrente I0
    	netlist[ne_extra].nome="IDS"+ne_extra;
    	netlist[ne_extra].a=numero(na);
    	netlist[ne_extra].c=numero(nc);
    	ne_extra++;
    	//transcondutancia GmVGS
    	netlist[ne_extra].nome="GmVGS"+ne_extra;
    	netlist[ne_extra].a=numero(na);
    	netlist[ne_extra].c=numero(nc);
    	ne_extra++;
    	//transcondutancia GmbVBS
    	netlist[ne_extra].nome="GmbVBS"+ne_extra;
    	netlist[ne_extra].a=numero(na);
    	netlist[ne_extra].c=numero(nc);
    	ne_extra++;
    	//capacitancia CGD: vira resistor RGD
    	netlist[ne_extra].nome="RCGD"+ne_extra;
    	netlist[ne_extra].b=numero(nb);
    	netlist[ne_extra].a=numero(na);
    	netlist[ne_extra].valor=1e9;
    	ne_extra++;
    	//capacitancia CGS: vira resistor RGS
    	netlist[ne_extra].nome="RCGS"+ne_extra;
    	netlist[ne_extra].b=numero(nb);
    	netlist[ne_extra].c=numero(nc);
    	netlist[ne_extra].valor=1e9;
    	ne_extra++;
		//capacitancia CGB: vira resistor RGB
    	netlist[ne_extra].nome="RCGB"+ne_extra;
    	netlist[ne_extra].b=numero(nb);
    	netlist[ne_extra].d=numero(nd);
    	netlist[ne_extra].valor=1e9;
    	ne_extra++;    	
    		
		}
        
    }
    
    else if (tipo=='*') { /* Comentario comeca com "*" */
      printf("Comentario: %s",txt);
      ne--;
    }
    else {
      printf("Elemento desconhecido: %s\n",txt);
      getch();
      exit(1);
    }
  }
 
  
  fclose(arquivo);
  /* Acrescenta variaveis de corrente acima dos nos, anotando no netlist */
  nn=nv;
  for (i=1; i<=ne; i++) {
    tipo=netlist[i].nome[0];
    if (tipo=='V' || tipo=='E' || tipo=='F' || tipo=='O') {
      nv++;
      if (nv>MAX_NOS) {
        printf("As correntes extra excederam o numero de variaveis permitido (%d)\n",MAX_NOS);
        exit(1);
      }
      strcpy(lista[nv],"j"); /* Tem espaco para mais dois caracteres */
      strcat(lista[nv],netlist[i].nome);
      netlist[i].x=nv;
    }
    else if (tipo=='H') {
      nv=nv+2;
      if (nv>MAX_NOS) {
        printf("As correntes extra excederam o numero de variaveis permitido (%d)\n",MAX_NOS);
        exit(1);
      }
      strcpy(lista[nv-1],"jx"); strcat(lista[nv-1],netlist[i].nome);
      netlist[i].x=nv-1;
      strcpy(lista[nv],"jy"); strcat(lista[nv],netlist[i].nome);
      netlist[i].y=nv;
    }
  }
  getch();
  /* Lista tudo */
  printf("Variaveis internas: \n");
  for (i=0; i<=nv; i++)
    printf("%d -> %s\n",i,lista[i]);
  getch();
  printf("Netlist interno final:\n");
  for (i=1; i<=ne; i++) {
    tipo=netlist[i].nome[0];
    if (tipo=='R' || tipo=='L' || tipo=='C' || tipo=='I' || tipo=='V') {
      printf("%s %d %d %g\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].valor);
    }
    else if (tipo=='G' || tipo=='E' || tipo=='F' || tipo=='H') {
      printf("%s %d %d %d %d %g\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].c,netlist[i].d,netlist[i].valor);
    }
    else if (tipo=='O') {
      printf("%s %d %d %d %d\n",netlist[i].nome,netlist[i].a,netlist[i].b,netlist[i].c,netlist[i].d);
    }
    else if (tipo=='K') {
      printf("%s %s %s %g\n",netlist[i].nome,acop_K.lA,acop_K.lB,netlist[i].valor);	
    else if (tipo=='M') {
      //implementar estrutura de repet. aqui! 	
      printf("%s %s %s %s",netlist[i].nome,netlist[i].a,netlist[i].c,calcular pelo NP);
	  printf("%s %s %s %s",netlist[i+1].nome,netlist[i+1].a,netlist[i+1].c,calcular pelo NP);
	  printf("%s %s %s %s",netlist[i+2].nome,netlist[i+2].a,netlist[i+2].c,calcular pelo NP);
	  printf("%s %s %s %s",netlist[i+3].nome,netlist[i+3].a,netlist[i+3].c,calcular pelo NP);
	  
	  printf("%s %s %s %g",netlist[i+4].nome,netlist[i+3].b,netlist[i+3].a,netlist[i+4].valor);
	  
	  printf("%s %s %s %g",netlist[i+5].nome,netlist[i+3].b,netlist[i+3].c,netlist[i+5].valor);
	  
	  printf("%s %s %s %g",netlist[i+6].nome,netlist[i+3].b,netlist[i+3].d,netlist[i+6].valor); 
	  	
	}  
	}
    if (tipo=='V' || tipo=='E' || tipo=='F' || tipo=='O')
      printf("Corrente jx: %d\n",netlist[i].x);
    else if (tipo=='H')
      printf("Correntes jx e jy: %d, %d\n",netlist[i].x,netlist[i].y);
  }
  getch();
  /* Monta o sistema nodal modificado */
  if(nao_linear>0)
  	printf("O circuito é não linear. Seu modelo linearizado tem %d nos, %d variaveis, %d elementos lineares e %d elementos não lineares (que se decompõe em %d elementos linearizados).\n",nn,nv,ne,nao_linear,ne_extra);
  else
  	printf("O circuito é linear.  Tem %d nos, %d variaveis e %d elementos\n",nn,nv,ne);
  getch();
  /* Zera sistema */
  for (i=0; i<=nv; i++) {
    for (j=0; j<=nv+1; j++)
      Yn[i][j]=0;
  }
  /* Monta estampas */
  for (i=1; i<=ne; i++) {
    tipo=netlist[i].nome[0];
    if (tipo=='R' || tipo=='L' || tipo=='C' ) {
      g=1/netlist[i].valor;
      Yn[netlist[i].a][netlist[i].a]+=g;
      Yn[netlist[i].b][netlist[i].b]+=g;
      Yn[netlist[i].a][netlist[i].b]-=g;
      Yn[netlist[i].b][netlist[i].a]-=g;
    }
    else if (tipo=='G') {
      g=netlist[i].valor;
      Yn[netlist[i].a][netlist[i].c]+=g;
      Yn[netlist[i].b][netlist[i].d]+=g;
      Yn[netlist[i].a][netlist[i].d]-=g;
      Yn[netlist[i].b][netlist[i].c]-=g;
    }
    else if (tipo=='I') {
      g=netlist[i].valor;
      Yn[netlist[i].a][nv+1]-=g;
      Yn[netlist[i].b][nv+1]+=g;
    }
    else if (tipo=='V') {
      Yn[netlist[i].a][netlist[i].x]+=1;
      Yn[netlist[i].b][netlist[i].x]-=1;
      Yn[netlist[i].x][netlist[i].a]-=1;
      Yn[netlist[i].x][netlist[i].b]+=1;
      Yn[netlist[i].x][nv+1]-=netlist[i].valor;
    }
    else if (tipo=='E') {
      g=netlist[i].valor;
      Yn[netlist[i].a][netlist[i].x]+=1;
      Yn[netlist[i].b][netlist[i].x]-=1;
      Yn[netlist[i].x][netlist[i].a]-=1;
      Yn[netlist[i].x][netlist[i].b]+=1;
      Yn[netlist[i].x][netlist[i].c]+=g;
      Yn[netlist[i].x][netlist[i].d]-=g;
    }
    else if (tipo=='F') {
      g=netlist[i].valor;
      Yn[netlist[i].a][netlist[i].x]+=g;
      Yn[netlist[i].b][netlist[i].x]-=g;
      Yn[netlist[i].c][netlist[i].x]+=1;
      Yn[netlist[i].d][netlist[i].x]-=1;
      Yn[netlist[i].x][netlist[i].c]-=1;
      Yn[netlist[i].x][netlist[i].d]+=1;
    }
    else if (tipo=='H') {
      g=netlist[i].valor;
      Yn[netlist[i].a][netlist[i].y]+=1;
      Yn[netlist[i].b][netlist[i].y]-=1;
      Yn[netlist[i].c][netlist[i].x]+=1;
      Yn[netlist[i].d][netlist[i].x]-=1;
      Yn[netlist[i].y][netlist[i].a]-=1;
      Yn[netlist[i].y][netlist[i].b]+=1;
      Yn[netlist[i].x][netlist[i].c]-=1;
      Yn[netlist[i].x][netlist[i].d]+=1;
      Yn[netlist[i].y][netlist[i].x]+=g;
    }
    else if (tipo=='O') {
      Yn[netlist[i].a][netlist[i].x]+=1;
      Yn[netlist[i].b][netlist[i].x]-=1;
      Yn[netlist[i].x][netlist[i].c]+=1;
      Yn[netlist[i].x][netlist[i].d]-=1;
    }
	
	if (netlist[i].nome[0] != 'K') {
#ifdef DEBUG
	    /* Opcional: Mostra o sistema apos a montagem da estampa */
	    printf("Sistema apos a estampa de %s\n",netlist[i].nome);
	    for (k=1; k<=nv; k++) {
	      for (j=1; j<=nv+1; j++)
	        if (Yn[k][j]!=0) printf("%+3.1f ",Yn[k][j]);
	        else printf(" ... ");
	      printf("\n");
	    }
	    getch();
	}
#endif

  }
  /* Resolve o sistema */
  if (resolversistema()) {
    getch();
    exit;
  }
#ifdef DEBUG
  /* Opcional: Mostra o sistema resolvido */
  printf("Sistema resolvido:\n");
  for (i=1; i<=nv; i++) {
      for (j=1; j<=nv+1; j++)
        if (Yn[i][j]!=0) printf("%+3.1f ",Yn[i][j]);
        else printf(" ... ");
      printf("\n");
    }
  getch();
#endif
  /* Mostra solucao */
  printf("Solucao:\n");
  strcpy(txt,"Tensao");
  for (i=1; i<=nv; i++) {
    if (i==nn+1) strcpy(txt,"Corrente");
    printf("%s %s: %g\n",txt,lista[i],Yn[i][nv+1]);
  }
  getch();
  return 0;
}
