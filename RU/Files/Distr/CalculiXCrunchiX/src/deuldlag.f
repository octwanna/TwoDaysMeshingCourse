!
!     CalculiX - A 3-dimensional finite element program
!              Copyright (C) 1998-2013 Guido Dhondt
!
!     This program is free software; you can redistribute it and/or
!     modify it under the terms of the GNU General Public License as
!     published by the Free Software Foundation(version 2);
!     
!
!     This program is distributed in the hope that it will be useful,
!     but WITHOUT ANY WARRANTY; without even the implied warranty of 
!     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
!     GNU General Public License for more details.
!
!     You should have received a copy of the GNU General Public License
!     along with this program; if not, write to the Free Software
!     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
!
      subroutine deuldlag(xi,et,ze,xlag,xeul,xj,xs)
!
!     calculation of the coefficients of the linearization
!     of J:=det(dx/dX)=1 at (xi,et,ze) for a 20-node quadratic
!     isoparametric brick element, -1<=xi,et,ze<=1
!     xlag are Lagrangian coordinates, xeul are Eulerian coordinates
!
      implicit none
!
      integer i,j,k
!
      real*8 xs(3,3),xlag(3,20),shpe(4,20),dd1,dd2,dd3,xeul(3,20)
!
      real*8 xi,et,ze,xj,omg,omh,omr,opg,oph,opr,
     &  tpgphpr,tmgphpr,tmgmhpr,tpgmhpr,tpgphmr,tmgphmr,tmgmhmr,tpgmhmr,
     &  omgopg,omhoph,omropr,omgmopg,omhmoph,omrmopr
!
!     shape functions and their glocal derivatives
!
      omg=1.d0-xi
      omh=1.d0-et
      omr=1.d0-ze
      opg=1.d0+xi
      oph=1.d0+et
      opr=1.d0+ze
      tpgphpr=opg+oph+ze
      tmgphpr=omg+oph+ze
      tmgmhpr=omg+omh+ze
      tpgmhpr=opg+omh+ze
      tpgphmr=opg+oph-ze
      tmgphmr=omg+oph-ze
      tmgmhmr=omg+omh-ze
      tpgmhmr=opg+omh-ze
      omgopg=omg*opg/4.d0
      omhoph=omh*oph/4.d0
      omropr=omr*opr/4.d0
      omgmopg=(omg-opg)/4.d0
      omhmoph=(omh-oph)/4.d0
      omrmopr=(omr-opr)/4.d0
!
!     local derivatives of the shape functions: xi-derivative
!
      shpe(1, 1)=omh*omr*(tpgphpr-omg)/8.d0
      shpe(1, 2)=(opg-tmgphpr)*omh*omr/8.d0
      shpe(1, 3)=(opg-tmgmhpr)*oph*omr/8.d0
      shpe(1, 4)=oph*omr*(tpgmhpr-omg)/8.d0
      shpe(1, 5)=omh*opr*(tpgphmr-omg)/8.d0
      shpe(1, 6)=(opg-tmgphmr)*omh*opr/8.d0
      shpe(1, 7)=(opg-tmgmhmr)*oph*opr/8.d0
      shpe(1, 8)=oph*opr*(tpgmhmr-omg)/8.d0
      shpe(1, 9)=omgmopg*omh*omr
      shpe(1,10)=omhoph*omr
      shpe(1,11)=omgmopg*oph*omr
      shpe(1,12)=-omhoph*omr
      shpe(1,13)=omgmopg*omh*opr
      shpe(1,14)=omhoph*opr
      shpe(1,15)=omgmopg*oph*opr
      shpe(1,16)=-omhoph*opr
      shpe(1,17)=-omropr*omh
      shpe(1,18)=omropr*omh
      shpe(1,19)=omropr*oph
      shpe(1,20)=-omropr*oph
!
!     local derivatives of the shape functions: eta-derivative
!
      shpe(2, 1)=omg*omr*(tpgphpr-omh)/8.d0
      shpe(2, 2)=opg*omr*(tmgphpr-omh)/8.d0
      shpe(2, 3)=opg*(oph-tmgmhpr)*omr/8.d0
      shpe(2, 4)=omg*(oph-tpgmhpr)*omr/8.d0
      shpe(2, 5)=omg*opr*(tpgphmr-omh)/8.d0
      shpe(2, 6)=opg*opr*(tmgphmr-omh)/8.d0
      shpe(2, 7)=opg*(oph-tmgmhmr)*opr/8.d0
      shpe(2, 8)=omg*(oph-tpgmhmr)*opr/8.d0
      shpe(2, 9)=-omgopg*omr
      shpe(2,10)=omhmoph*opg*omr
      shpe(2,11)=omgopg*omr
      shpe(2,12)=omhmoph*omg*omr
      shpe(2,13)=-omgopg*opr
      shpe(2,14)=omhmoph*opg*opr
      shpe(2,15)=omgopg*opr
      shpe(2,16)=omhmoph*omg*opr
      shpe(2,17)=-omropr*omg
      shpe(2,18)=-omropr*opg
      shpe(2,19)=omropr*opg
      shpe(2,20)=omropr*omg
!
!     local derivatives of the shape functions: zeta-derivative
!
      shpe(3, 1)=omg*omh*(tpgphpr-omr)/8.d0
      shpe(3, 2)=opg*omh*(tmgphpr-omr)/8.d0
      shpe(3, 3)=opg*oph*(tmgmhpr-omr)/8.d0
      shpe(3, 4)=omg*oph*(tpgmhpr-omr)/8.d0
      shpe(3, 5)=omg*omh*(opr-tpgphmr)/8.d0
      shpe(3, 6)=opg*omh*(opr-tmgphmr)/8.d0
      shpe(3, 7)=opg*oph*(opr-tmgmhmr)/8.d0
      shpe(3, 8)=omg*oph*(opr-tpgmhmr)/8.d0
      shpe(3, 9)=-omgopg*omh
      shpe(3,10)=-omhoph*opg
      shpe(3,11)=-omgopg*oph
      shpe(3,12)=-omhoph*omg
      shpe(3,13)=omgopg*omh
      shpe(3,14)=omhoph*opg
      shpe(3,15)=omgopg*oph
      shpe(3,16)=omhoph*omg
      shpe(3,17)=omrmopr*omg*omh
      shpe(3,18)=omrmopr*opg*omh
      shpe(3,19)=omrmopr*opg*oph
      shpe(3,20)=omrmopr*omg*oph
!
!     computation of the derivative of the global 
!     material coordinates w.r.t. the local coordinates
!
      do i=1,3
        do j=1,3
          xs(i,j)=0.d0
          do k=1,20
            xs(i,j)=xs(i,j)+xlag(i,k)*shpe(j,k)
          enddo
        enddo
      enddo
!
!     computation of the jacobian determinant of the local
!     coordinates w.r.t. the global material coordinates
!
      dd1=xs(2,2)*xs(3,3)-xs(2,3)*xs(3,2)
      dd2=xs(2,3)*xs(3,1)-xs(2,1)*xs(3,3)
      dd3=xs(2,1)*xs(3,2)-xs(2,2)*xs(3,1)
      xj=xs(1,1)*dd1+xs(1,2)*dd2+xs(1,3)*dd3
      xj=1.d0/xj
!
!     computation of the derivative of the global 
!     spatial coordinates w.r.t. the local coordinates
!
      do i=1,3
        do j=1,3
          xs(i,j)=0.d0
          do k=1,20
            xs(i,j)=xs(i,j)+xeul(i,k)*shpe(j,k)
          enddo
        enddo
      enddo
!
      return
      end
