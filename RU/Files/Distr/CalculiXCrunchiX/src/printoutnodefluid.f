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
      subroutine printoutnodefluid(prlab,v,vold,vcontu,physcon,ii,node,
     &  trab,inotr,ntrans,co,mi)
!
!     stores results in the .dat file
!
      implicit none
!
      character*6 prlab(*)
!
      integer node,ii,j,inotr(2,*),ntrans,mi(*)
!
      real*8 v(0:mi(2),*),trab(7,*),
     &  co(3,*),a(3,3),vcontu(2,*),physcon(*),vold(0:mi(2),*)
!
      if(prlab(ii)(1:4).eq.'VF  ') then
         if((ntrans.eq.0).or.(prlab(ii)(6:6).eq.'G')) then
            write(5,'(i10,1p,3(1x,e13.6))') node,
     &           (vold(j,node),j=1,3)
         elseif(inotr(1,node).eq.0) then
            write(5,'(i10,1p,3(1x,e13.6))') node,
     &           (vold(j,node),j=1,3)
         else
            call transformatrix(trab(1,inotr(1,node)),co(1,node),a)
            write(5,'(i10,1p,3(1x,e13.6))') node,
     &      vold(1,node)*a(1,1)+vold(2,node)*a(2,1)+vold(3,node)*a(3,1),
     &      vold(1,node)*a(1,2)+vold(2,node)*a(2,2)+vold(3,node)*a(3,2),
     &      vold(1,node)*a(1,3)+vold(2,node)*a(2,3)+vold(3,node)*a(3,3)
         endif
      elseif(prlab(ii)(1:4).eq.'PSF ') then
         write(5,'(i10,1x,1p,e13.6)') node,
     &           vold(4,node)
      elseif(prlab(ii)(1:4).eq.'TSF ') then
         write(5,'(i10,1x,1p,e13.6)') node,
     &           vold(0,node)
      elseif(prlab(ii)(1:4).eq.'PTF ') then
         write(5,'(i10,1x,1p,e13.6)') node,vold(4,node)*
     &        (1.d0+(v(0,node)-1.d0)/2*v(1,node)**2)**(v(0,node)/
     &        (v(0,node)-1.d0))
      elseif(prlab(ii)(1:4).eq.'TTF ') then
         write(5,'(i10,1x,1p,e13.6)') node,
     &     vold(0,node)*(1.d0+(v(0,node)-1.d0)/2*v(1,node)**2)
      elseif(prlab(ii)(1:4).eq.'CP  ') then
         write(5,'(i10,1x,1p,e13.6)') node,
     &            (vold(4,node)-physcon(6))*2.d0/
     &            (physcon(7)*physcon(5)**2)
      elseif(prlab(ii)(1:4).eq.'TURB') then
         write(5,'(i10,1x,1p,e13.6,1p,e13.6)') node,
     &            vcontu(1,node),vcontu(2,node)
      endif
!
      flush(5)
!
      return
      end






