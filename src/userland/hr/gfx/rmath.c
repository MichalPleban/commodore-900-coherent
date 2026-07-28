#include <stdio.h>
#include "smgr.h"

RECT
R_Intersection(s,t)
register RECT s,t;
	{ 
	RECT r;

	r.origin.x = s.origin.x > t.origin.x ? s.origin.x : t.origin.x;
	  r.origin.y=s.origin.y>t.origin.y?s.origin.y:t.origin.y;
	  r.corner.x=s.corner.x<t.corner.x?s.corner.x:t.corner.x;
	  r.corner.y=s.corner.y<t.corner.y?s.corner.y:t.corner.y;
	return(r);
 }


/*
**  Calculate the smallest rectangle that encloses rectangles 's' and 't'.
*/
RECT
R_merge(s,t)
register RECT s,t;
{
	RECT r;
	
	 r.origin.x=s.origin.x<t.origin.x?s.origin.x:t.origin.x;
	  r.origin.y=s.origin.y<t.origin.y?s.origin.y:t.origin.y;
	  r.corner.x=s.corner.x>t.corner.x?s.corner.x:t.corner.x;
	  r.corner.y=s.corner.y>t.corner.y?s.corner.y:t.corner.y;
	return(r);
 }
/*
**  Set rectangle 's' to a rectangle that includes the rectangle 'r' and point 'p'
*/
RECT 
R_incl_point(r,p)
register RECT r;
register POINT p;
{
	RECT s;
	s.origin.x = r.origin.x < p.x ? r.origin.x : p.x;
	s.origin.y = r.origin.y < p.y ? r.origin.y : p.y;
	s.corner.x = r.corner.x > p.x ? r.corner.x : p.x;
	s.corner.y = r.corner.y > p.y ? r.corner.y : p.y; 
	return(s);
}

/*
**  Inset the corners of the rectangle 'r' by 'dx' and 'dy'.
*/
RECT
R_inset(r,x,y)
register RECT r;
register int x,y;
	{
	RECT s;

	s.origin.x = r.origin.x + x; 
	s.origin.y = r.origin.y + y;
	s.corner.x = r.corner.x - x;
	s.corner.y = r.corner.y - y;
	return(s);
	}


/*
**  Calculate the translation of 'r' by adding point 'p'.
*/
RECT
R_addp(r, p)	
register RECT r;
register POINT p;
	{
	RECT s;

	s.origin.x = r.origin.x + p.x;
	s.origin.y = r.origin.y + p.y;
	s.corner.x = r.corner.x + p.x; 
	s.corner.y = r.corner.y + p.y;

	return(s);
	}

/*
**  Calculate the translation of 'r' by subtracting point 'p'.
*/
RECT 
R_subp(r, p)	
register RECT r;
register POINT p;
	{
	RECT s;

	s.origin.x = r.origin.x - p.x;
	s.origin.y = r.origin.y - p.y;
	s.corner.x = r.corner.x - p.x;
	s.corner.y = r.corner.y - p.y;
	
	return(s);
	}


/*
**  Calculate the translation of 'r' by a displacement 'dx' and 'dy'.
*/
RECT
R_offset(r, x, y) 
register RECT r;
register int x,y;
	{
	RECT s;

	s.origin.x = r.origin.x + x;
	s.origin.y = r.origin.y + y;
	s.corner.x = r.corner.x + x; 
	s.corner.y = r.corner.y + y;

	return(s);
	}

/*
**  Do rectangles 'r' and 's' intersect?
*/
bool R_intersect(r,s)
register RECT r,s;
	{
 	return ( r.origin.x < s.corner.x &&
	    	 s.origin.x < r.corner.x &&
		 r.origin.y < s.corner.y && 
	    	 s.origin.y < r.corner.y );
	}

/*
**  Are rectangles 'r' and 's' equal?
*/
bool R_equal(r,s)
register RECT r,s;

	{
	return 	( r.origin.x == s.origin.x  && 
		  r.origin.y == s.origin.y  && 
		  r.corner.x == s.corner.x  &&
		  r.corner.y == s.corner.y  );
	}
/*
*  Is point 'p' contained within rectangle 'r'?
*/
bool R_point_in(r, p)
register RECT r;
register POINT p;
	{
	return	( p.x >= r.origin.x &&
		  p.y >= r.origin.y &&
		  p.x <  r.corner.x && 
		  p.y <  r.corner.y );
	}

/*
**  Is rectangle 'r' empty?
*/
bool R_null(r)
register RECT r;
	{
	return	( r.origin.x >= r.corner.x || r.origin.y >= r.corner.y );
	}


/*
**  Set rectangle 'r' to an empty rectangle.
*/
RECT R_empty(r)	
register RECT r;
	{ 
	r.origin.x = r.origin.y = 0; 
	r.corner.x = r.corner.y = 0; 
	return(r);
	}

/* 
** Is rectangle r contained within rectangle s 
*/
bool R_subset(r,s)
register RECT r,s;
	{
	return ( (r.origin.x >= s.origin.x) &&
		 (r.origin.y >= s.origin.y) &&
		 (r.corner.x <= s.corner.x) &&
		 (r.corner.y <= s.corner.y) );
	}


