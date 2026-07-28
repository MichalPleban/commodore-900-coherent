# include <stdio.h>
# include "smgr.h"

extern int    ALL_ON;
extern char   *malloc();
extern int    *bmialloc();

FONT_HEADER   *aftable[FM_MAXFONT];

/***********************************************************************
      Font_test.c  allows  application programs the use of a wide 
      variety of  fonts  using minimal  amounts of system memory.
      Accessing a font is done by using the following Font_Mgr.c
      functions:   FM_open, FM_close, FM_getch, SM_GetInfo
***********************************************************************/


FM_open(ff_name)
char *ff_name;
{  
   register FONT_HEADER *aft_entry;
   register int i;
   FILE *fp;
   int  tbl_entrs,       /* number of table entries */
	fwords;          /* number of words in entire
			    bitmap image            */
   BLTSTRUCT blt;
	int	ferr;	/* font open error code...only for memory failure */
	
#ifdef LISA
	int pete;
#endif

#ifdef DEBUG
printf("***  FM_open: %s  ****\n", ff_name);
#endif

   /* for a few minutes here, fwords will be used to determine
    * whether or not there is an index available
    */
   fwords = 0;

      /* allocate storage for font header */
   if(!(aft_entry = (FONT_HEADER *) malloc(sizeof(FONT_HEADER))))
   {
      printf("FM_open: **** ERROR ****");
      printf("     memory allocation ** failed (aft_entry)\n\n");
      return(MEM_ALLOC_FAIL);
   }


      if(!(fp = fopen(ff_name,"r"))) /* input file read only */
      {
         printf("FM_open: **** ERROR ****");
         printf("     input file open ** failed\n\n");
         free(aft_entry);
         return(FILE_OPEN_FAIL);
      }

         /* load the FONT_HEADER */
      	aft_entry->ffirst_char  = (int) getc(fp);   /* first char of font */
      	aft_entry->flast_char   = (int) getc(fp);   /* last char of font  */
      	aft_entry->flast_char  &= 0xff;             /* possible sign extension*/

	/* read , in order, ascent, descent, leading, max_w, min_w,  */
	/* FM_MAXNLEN characters of name, 3 characters unused        */
	fread( &aft_entry->font_ascent, sizeof(char), 8+FM_MAXNLEN, fp);
      aft_entry->font_map.rect.origin.x = 0;
      aft_entry->font_map.rect.origin.y = 0;
      aft_entry->font_map.rect.corner.x = getw(fp);
      aft_entry->font_map.rect.corner.y = getw(fp);
      i = aft_entry->font_map.rect.corner.x;
      aft_entry->font_map.width = (i % 16)? (i/16+1): (i/16);
      (aft_entry->font_map.width) *= 16;


   for(i = 0; i < FM_MAXFONT; i++)
      if(aftable[i])        /* is aftable[i] <> 0 */
         if(strcmp(aft_entry->ffont_name,aftable[i]->ffont_name) == 0)
         {
	    if ( i != 0 )
	    	aftable[i]->fopen_count++;
#ifdef DEBUG
	printf("---- FONT ALREADY OPEN ---- increment open count\n");
	printf("  aftable[%d]->fopen_count = %d\n",i,aftable[i]->fopen_count);
#endif
	    fclose(fp);
	    return(i);
         }


      for(i = 0; i < FM_MAXFONT; i++)
         if(!(aftable[i]))   /* is aftable = 0 */
         {
	    fwords = 1;
	    break;
         }

      if ( fwords == 0 )
      {
       printf("FM_open: **** ERROR ****");
       printf("     maximum number of (8) fonts already loaded\n\n");
       fclose(fp);
       return(TAB_FILLED);
      }

#ifdef DEBUG
       printf("---- FONT NOT CURRENTLY IN USE ----\n");
       printf("   aftable[%d]  * available *\n\n",i);
#endif
   
      aftable[i] = aft_entry;
   
      aft_entry->fopen_count = 1;
      tbl_entrs = (aft_entry->flast_char) - (aft_entry->ffirst_char) + 3;
   
#ifdef DEBUG
printf("HEADER COPY:\n");
printf("   first=%d, last=%d\n",aft_entry->ffirst_char,aft_entry->flast_char);
printf("   ascent=%d, descent=%d\n",aft_entry->font_ascent,aft_entry->font_descent);
printf("   leading=%d, maxwidth=%d\n",aft_entry->font_leading,aft_entry->fmax_width);
printf("   minwidth=%d, fid=%d, fontname=%s\n",aft_entry->fmin_width,i,aft_entry->ffont_name);
printf("   tbl_entrs = %d, open_count = %d\n\n",tbl_entrs,aft_entry->fopen_count);
#endif
   
            /* allocate storage for location table */
      if(!(aft_entry->loctable = (int *) malloc(tbl_entrs*2)))
      {
	ferr = 3;
	goto font_error;
      }
   
            /* allocate storage for kern/width table */
      if(!(aft_entry->kwtable = (int *) malloc(tbl_entrs*2)))
      {
	ferr = 4;
	goto font_error;
      }
   
            /* allocate storage for bitmap */
      aft_entry->font_map.base = bmialloc(aft_entry->font_map.rect);
      if ( aft_entry->font_map.base == (int *)NULL )
	{
	ferr = 5;
	goto font_error;
	}
   
         fwords = ((aft_entry->font_map.width) / 16) * 
		   (aft_entry->font_map.rect.corner.y);
   
	fread(aft_entry->loctable, sizeof(int), tbl_entrs, fp);
#ifdef LISA
	for ( pete = 0; pete < tbl_entrs; pete++ )
	peteprint(" entry #%d location %d\n", pete, aft_entry->loctable[pete]);
#endif
   	fread(aft_entry->kwtable, sizeof(int), tbl_entrs, fp);
#ifdef LISA
	for ( pete = 0; pete < tbl_entrs; pete++ )
	peteprint(" entry #%d kw %d\n", pete, aft_entry->kwtable[pete]);
#endif
	if ( fread(aft_entry->font_map.base, sizeof(int), fwords, fp) != fwords)
		peteprint("bitmap mismatch\n");

	 blt.src = blt.dst = &(aft_entry->font_map);
	 blt.sp = aft_entry->font_map.rect.origin;
	 blt.dr = aft_entry->font_map.rect;
	 blt.op = L_NDST;
	 blt.pat = &ALL_ON;
	 bitblt(&blt, 1, 1);

         fclose(fp);
         return(i);

font_error:
	if ( ferr > 4 )
		free( aft_entry->kwtable );
	if ( ferr > 3 )
		free( aft_entry->loctable );
	if ( ferr > 2 )
		free( aft_entry );
	
	aftable[i] = (FONT_HEADER *)NULL;
	close(fp);
	return(MEM_ALLOC_FAIL);
}
   
   
FM_getch(fmptr)
register GETPTR  *fmptr;
{
	FONT_HEADER *aft_entry;
	FONT_HEADER aft;
	register int pos, npos;
	int 	chr_entrs,  /* number of entries in table            */
     		char_width, /* width of characters in pixels         */
     		ffid;

#ifdef DEBUG
   printf("gfid = %d, gch = %d\n",fmptr->gfid,fmptr->gch);
#endif

	ffid = fmptr->gfid;

   if( (ffid >=FM_MAXFONT) || (ffid < 0 ) || (!(aftable[ffid])) ) 
      return(FONT_NOT_OPEN);

   aft_entry = aftable[ffid];

#ifdef DEBUG
   printf("aftable[%d] = %lx\n\n",ffid,aft_entry);
#endif 

   chr_entrs = (aft_entry->flast_char) - (aft_entry->ffirst_char);
   pos = (fmptr->gch) - (aft_entry->ffirst_char);

   for(npos = pos+1; aft_entry->kwtable[npos] == FNOT_EXST; npos++)
	;
   if((pos < 0) || (pos > chr_entrs))
   {

#ifdef DEBUG
printf("FM_getch:  ****WARNING ****\n");
printf("         character does not exist in font\n\n");
#endif

      pos = (aft_entry->flast_char) - (aft_entry->ffirst_char) + 1;
      npos = pos + 1;
   }
   if(aft_entry->kwtable[pos] == FNOT_EXST)
   {

#ifdef DEBUG
printf("FM_getch:  **** ERROR ****\n");
printf("         character does not exist in font\n\n");
#endif

      
      pos = (aft_entry->flast_char) - (aft_entry->ffirst_char) + 1;
      npos = pos + 1;
   }

   fmptr->gbmap = aft_entry->font_map;
   fmptr->gpoint.x = aft_entry->loctable[pos];
   fmptr->gpoint.y = 0;
   fmptr->gkern = (aft_entry->kwtable[pos]) >> 8;
   char_width = (aft_entry->kwtable[pos]) & (0x00FF);
   fmptr->gascent = aft_entry->font_ascent;
   fmptr->gdescent = aft_entry->font_descent;
   fmptr->gleading = aft_entry->font_leading;
   fmptr->gawdth = (aft_entry->loctable[npos]) - (aft_entry->loctable[pos]);


#ifdef DEBUG
   printf("!!!!  AFTER ASSIGNMENTS HAVE BEEN MADE !!!\n");
   printf("gfid = %d, gch = %d\n",fmptr->gfid,fmptr->gch);
   printf("gbmap = 0x%lx, gpoint = 0x%lx\n",fmptr->gbmap,fmptr->gpoint);
   printf("******* BITMAP ********\n");
   printf("bbase = 0x%lx, bwidth = %d\n",fmptr->gbmap.base,fmptr->gbmap.width);
   printf("boriginx = %d, boriginy = %d\n",fmptr->gbmap.rect.origin.x,fmptr->gbmap.rect.origin.y);
   printf("bcornerx = %d, bcornery = %d\n",fmptr->gbmap.rect.corner.x,fmptr->gbmap.rect.corner.y);
   printf("bpointx = %d, bpointy = %d\n",fmptr->gpoint.x,fmptr->gpoint.y);
   printf("gascent = %d, gdescent = %d\n",fmptr->gascent,fmptr->gdescent);
   printf("gleading = %d, gawdth = %d\n\n",fmptr->gleading,fmptr->gawdth);
   printf("character %d, pos = %d, char_width = %d\n",fmptr->gch,pos,char_width);
   printf("kern = %d, actual width = %d\n\n",fmptr->gkern,fmptr->gawdth);
#endif

   return(char_width);
}


FM_close(cfid)
register int cfid;                     /* font id number            */
{
register FONT_HEADER *aft_entry;

#ifdef DEBUG
printf("Font_close: %d\n", cfid);
#endif

	if ( cfid == 0 )
		return(FCLOSE_OK);
   
   	if ( (cfid < 0 ) || (cfid >= FM_MAXFONT) || (!(aftable[cfid])) ) 
      		return(FNOT_EXST);

     	aft_entry = aftable[cfid];
     	aft_entry->fopen_count--;
     	if(!(aft_entry->fopen_count))  /* is open count = 0  */
	{
    		free(aft_entry->font_map.base);
    		free(aft_entry->kwtable);
    		free(aft_entry->loctable);
    		free(aft_entry);
    		aftable[cfid] = (FONT_HEADER *) FONT_NULL;
	}
      	return(FCLOSE_OK);
   	
}



FONT_HEADER *FM_GetInfo(fid)
register int	fid;
{

	if ( (fid >= FM_MAXFONT) || ( fid < 0) )
		return ( (char*)NULL );

	return ( aftable[fid] );
}
