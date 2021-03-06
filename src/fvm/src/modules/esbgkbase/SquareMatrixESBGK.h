// This file os part of FVM
// Copyright (c) 2012 FVM Authors
// See LICENSE file for terms.

#ifndef _SQUAREMATRIXESBGK_H_
#define _SQUAREMATRIXESBGK_H_

#include "Array.h"
#include "MatrixJML.h"
#include <math.h>
#include <omp.h>

template<class T>
class SquareMatrixESBGK : public MatrixJML<T>
{
 public:
  typedef Array<T> TArray;
  typedef Array<int> IntArray;
  typedef typename NumTypeTraits<T>::T_Scalar T_Scalar;

 SquareMatrixESBGK(const int N):
  _order(N),
    _elements(N*N),
    _sorted(false),
    _pivotRows(N),
    _maxVals(N),
    _values(_elements)
    {_values.zero();}
  
  T& getElement(const int i, const int j) {return _values[(i-1)*_order+j-1];}
  T& operator()(const int i, const int j) {return _values[(i-1)*_order+j-1];}
  void zero() {_values.zero();}
  
  void Solve(TArray& bVec)
  {//Gaussian Elimination w/ scaled partial pivoting
   //replaces bVec with the solution vector. 

    SquareMatrixESBGK<T> LU(_order);
    (*this).makeCopy(LU);
    IntArray l(_order);
    TArray s(_order);
    TArray x(_order);

    //find max values in each row if not done yet
    if(!_sorted)
    {
	for(int i=1;i<_order+1;i++)
	{
	    l[i-1]=i;
	    s[i-1]=fabs((*this)(i,1));
	    for(int j=2;j<_order+1;j++)
	    {
		if(s[i-1]<fabs((*this)(i,j)))
		  s[i-1]=fabs((*this)(i,j));
	    }
	}
	_maxVals=s;
    }
    
    //Forward sweep
    if(!_sorted)
    {
	for(int i=1;i<_order;i++)
	{
	    //T rmax=0;
	    T rmax=fabs(LU(l[i-1],i)/s[l[i-1]]);
	    int newMax=i;
	    for(int j=i+1;j<_order+1;j++)
	    {
		T r=fabs(LU(l[j-1],i)/s[l[j-1]]);
		if(r>rmax)
		{
		    rmax=r;
		    newMax=j;
		}
	    }

	    /*
	    int temp=l[i-1];
	    l[i-1]=l[newMax-1];
	    l[newMax-1]=temp;
	    */	    

	    //#pragma omp parallel for
	    for(int j=i+1;j<_order+1;j++)
	    {
		T factor=LU(l[j-1],i)/LU(l[i-1],i);
		LU(l[j-1],i)=factor;
		if(factor!=factor)
		  cout<<"GE NaN at l[j-1], l[i-1], LU(l[j-1],i), LU(l[i-1],i) "<<l[j-1]<<" "<<l[i-1]<<" "<<LU(l[j-1],i)<<" "<<LU(l[i-1],i)<<endl;
		bVec[l[j-1]-1]-=factor*bVec[l[i-1]-1];
		for(int k=i+1;k<_order+1;k++)
		  LU(l[j-1],k)=LU(l[j-1],k)-LU(l[i-1],k)*factor;
	    }
	}
	_pivotRows=l;
	_sorted=true;
    }
    else
      {
	for(int i=1;i<_order;i++)
	  {
	    for(int j=i+1;j<_order+1;j++)
	      {
		T factor=LU(_pivotRows[j-1],i)/LU(_pivotRows[i-1],i);
		LU(l[j-1],i)=factor;
		bVec[_pivotRows[j-1]-1]-=factor*bVec[_pivotRows[i-1]-1];
		for(int k=i+1;k<_order+1;k++)
		  {
		    LU(_pivotRows[j-1],k)=LU(_pivotRows[j-1],k)
		      -LU(_pivotRows[i-1],k)*factor;
		  }
	      }
	  }
      }


    
    //back solve
    /*        
    bVec[_pivotRows[_order-1]-1]=
      bVec[_pivotRows[_order-1]-1]/LU(_pivotRows[_order-1],_order);
    
    if(bVec[_pivotRows[_order-1]-1]!=bVec[_pivotRows[_order-1]-1])
      cout<<"bvec crashes at order"<<endl;
    T sum=0.;
    for(int i=_order-1;i>0;i--)
    {
	sum=0.;
	for(int j=i+1;j<_order+1;j++)
	  sum-=LU(_pivotRows[i-1],j)*bVec[j-1];
	bVec[_pivotRows[i-1]-1]+=sum;
	bVec[_pivotRows[i-1]-1]=bVec[_pivotRows[i-1]-1]/LU(_pivotRows[i-1],i);
	if(bVec[_pivotRows[i-1]-1]!=bVec[_pivotRows[i-1]-1])
	  cout<<"GE Error at "<<i<<endl;
    }
    */

    //************back solve***************//
    
    //initalize solution vector
    const T zero(0.);
    for(int i=0;i<_order;i++)
      x[i]=zero;

    x[_order-1]=bVec[_pivotRows[_order-1]-1]/LU(_pivotRows[_order-1],_order);

    T sum=0.;
    for(int i=_order-1;i>0;i--)
    {
        sum=bVec[_pivotRows[i-1]-1];
        for(int j=i+1;j<_order+1;j++)
          sum=sum-LU(_pivotRows[i-1],j)*x[j-1];
        x[i-1]=sum/LU(_pivotRows[i-1],i);
    }
    bVec=x;
    
    //***************************//

  }

  void makeCopy(SquareMatrixESBGK<T>& o)
  {
    if(o._order!=this->_order)
      throw CException("Cannot copy matrices of different sizes!");

    o._sorted=this->_sorted;
    o._pivotRows=this->_pivotRows;
    o._maxVals=this->_maxVals;
    o._values=this->_values;
  }

  void printMatrix()
  {
    for(int i=1;i<_order+1;i++)
      {
	for(int j=1;j<_order+1;j++)
	  cout<<(*this)(i,j)<<" ";
	cout<<endl;
      }
    cout<<endl;
  }

  T getTraceAbs()
  {
    T trace=0.;
    for(int i=1;i<_order+1;i++)
      trace+=fabs((*this)(i,i));
    return trace;
  }

 private:
  const int _order;
  const int _elements;
  bool _sorted;
  IntArray _pivotRows;
  TArray _maxVals;
  TArray _values;
  

};


#endif
