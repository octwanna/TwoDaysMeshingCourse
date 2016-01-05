!
!     CalculiX - A 3-dimensional finite element program
!              Copyright (C) 1998-2007 Guido Dhondt
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
      subroutine springforc(xl,konl,vl,imat,elcon,nelcon,
     &  elas,fnl,ncmat_,ntmat_,nope,lakonl,t0l,t1l,kode,elconloc,
     &  plicon,nplicon,npmat_,veoldl,senergy,iener,cstr,mi,
     &  springarea,nmethod,ne0,iperturb,nstate_,xstateini,
     &  xstate,reltime,xnormastface,ielas)
!
!     calculates the force of the spring
!
      implicit none
!
      character*8 lakonl
!
      integer konl(9),i,j,imat,ncmat_,ntmat_,nope,nterms,iflag,mi(*),
     &  kode,niso,id,nplicon(0:ntmat_,*),npmat_,nelcon(2,*),iener,
     &  nmethod,ne0,iperturb(2),nstate_,ielas
!
      real*8 xl(3,10),elas(21),ratio(9),t0l,t1l,al(3),vl(0:mi(2),10),
     &  pl(3,10),xn(3),dm,alpha,beta,fnl(3,10),tp(3),te(3),ftrial(3),
     &  veoldl(0:mi(2),10),dist,c2,c3,t(3),dt,dftrial,vertan(3),
     &  elcon(0:ncmat_,ntmat_,*),pproj(3),xsj2(3),xs2(3,7),clear,
     &  shp2(7,9),xi,et,elconloc(21),plconloc(802),xk,fk,dd,val,
     &  xiso(200),yiso(200),dd0,plicon(0:2*npmat_,ntmat_,*),
     &  um,eps,pi,senergy,cstr(6),dvertan,dg,dfshear,dfnl,
     &  fricforc,springarea(2),ver(3),dvernor,overlap,pres,
     &  xstate(nstate_,mi(1),*),xstateini(nstate_,mi(1),*),t1(3),t2(3),
     &  dt1,dte,alnew(3),reltime,xnormastface(3,9),dm2
!
      iflag=2
!
!     actual positions of the nodes belonging to the contact spring
!     (otherwise no contact force)
!     
      if(nmethod.ne.2) then
         do i=1,nope
            do j=1,3
               pl(j,i)=xl(j,i)+vl(j,i)
            enddo
         enddo
      else
!
!        for frequency calculations the eigenmodes are freely
!        scalable, leading to problems with contact finding 
!
         do i=1,nope
            do j=1,3
               pl(j,i)=xl(j,i)
            enddo
         enddo
      endif
!     
      if(lakonl(7:7).eq.'A') then
         dd0=dsqrt((xl(1,2)-xl(1,1))**2
     &           +(xl(2,2)-xl(2,1))**2
     &           +(xl(3,2)-xl(3,1))**2)
         dd=dsqrt((pl(1,2)-pl(1,1))**2
     &           +(pl(2,2)-pl(2,1))**2
     &           +(pl(3,2)-pl(3,1))**2)
         do i=1,3
            xn(i)=(pl(i,2)-pl(i,1))/dd
         enddo
         val=dd-dd0
!
!        interpolating the material data
!
         call materialdata_sp(elcon,nelcon,imat,ntmat_,i,t1l,
     &     elconloc,kode,plicon,nplicon,npmat_,plconloc,ncmat_)
!
!        calculating the spring force and the spring constant
!
         if(kode.eq.2)then
            xk=elconloc(1)
            fk=xk*val
            if(iener.eq.1) then
               senergy=fk*val/2.d0
            endif
         else
            niso=int(plconloc(801))
            do i=1,niso
               xiso(i)=plconloc(2*i-1)
               yiso(i)=plconloc(2*i)
            enddo
            call ident(xiso,val,niso,id)
            if(id.eq.0) then
               xk=0.d0
               fk=yiso(1)
               if(iener.eq.1) then
                  senergy=fk*val;
               endif
            elseif(id.eq.niso) then
               xk=0.d0
               fk=yiso(niso)
               if(iener.eq.1) then
                  senergy=yiso(1)*xiso(1)
                  do i=2,niso
                     senergy=senergy+(xiso(i)-xiso(i-1))*(yiso(i)+yiso(
     &               i-1))/2.d0
                  enddo
                  senergy=senergy+(val-xiso(niso))*yiso(niso)
               endif
            else
               xk=(yiso(id+1)-yiso(id))/(xiso(id+1)-xiso(id))
               fk=yiso(id)+xk*(val-xiso(id))
               if(iener.eq.1) then
                  senergy=yiso(1)*xiso(1)
                  do i=2, id
                     senergy=senergy+(xiso(i)-xiso(i-1))*
     &                    (yiso(i)+yiso(i-1))/2.d0
                  enddo
                  senergy=senergy+(val-xiso(id))*(fk+yiso(id))/2.d0
               endif
            endif
         endif
!
         do i=1,3
            fnl(i,1)=-fk*xn(i)
            fnl(i,2)=fk*xn(i)
         enddo
         return
      endif
!
      nterms=nope-1
c      do i=1,nterms
c         write(*,*) 'springforc ',(pl(j,i),j=1,3),konl(i)
c      enddo
!
!     vector vr connects the dependent node with its projection
!     on the independent face
!
      do i=1,3
         pproj(i)=pl(i,nope)
      enddo
c      call attachpen(pl,pproj,nterms,ratio,dist,xi,et,xnormastface)
      call attach(pl,pproj,nterms,ratio,dist,xi,et)
      do i=1,3
         al(i)=pl(i,nope)-pproj(i)
      enddo
!
!     determining the jacobian vector on the surface 
!
      if(nterms.eq.9) then
         call shape9q(xi,et,pl,xsj2,xs2,shp2,iflag)
      elseif(nterms.eq.8) then
         call shape8q(xi,et,pl,xsj2,xs2,shp2,iflag)
      elseif(nterms.eq.4) then
         call shape4q(xi,et,pl,xsj2,xs2,shp2,iflag)
      elseif(nterms.eq.6) then
         call shape6tri(xi,et,pl,xsj2,xs2,shp2,iflag)
      elseif(nterms.eq.7) then
         call shape7tri(xi,et,pl,xsj2,xs2,shp2,iflag)
      else
         call shape3tri(xi,et,pl,xsj2,xs2,shp2,iflag)
      endif
c!
c!     normal vector in the projection point based on the
c!     edge normals of the master face
c!     
c      do i=1,3
c      	 xn(i)=0.d0
c	 do j=1,nterms
c	    xn(i)=xn(i)+ratio(j)*xnormastface(i,j)
c	 enddo
c      enddo
c!      
c      dm2=dsqrt(xn(1)*xn(1)+xn(2)*xn(2)+xn(3)*xn(3))
c      do i=1,3
c         xn(i)=xn(i)/dm2
c      enddo
c!
c      dm=dsqrt(xsj2(1)*xsj2(1)+xsj2(2)*xsj2(2)+xsj2(3)*xsj2(3))
!
!     normal on the surface
!
      dm=dsqrt(xsj2(1)*xsj2(1)+xsj2(2)*xsj2(2)+xsj2(3)*xsj2(3))
      do i=1,3
         xn(i)=xsj2(i)/dm
      enddo
!
!     distance from surface along normal (= clearance)
!
      clear=al(1)*xn(1)+al(2)*xn(2)+al(3)*xn(3)
!
!     check for a reduction of the initial penetration, if any
!
      if(nmethod.eq.1) then
         clear=clear-springarea(2)*(1.d0-reltime)
      endif
      if(clear.le.0.d0) cstr(1)=clear
!
!     representative area: usually the slave surface stored in
!     springarea; however, if no area was assigned because the
!     node does not belong to any element, the master surface
!     is used
!
      if(springarea(1).le.0.d0) then
         if(nterms.eq.3) then
            springarea(1)=dm/2.d0
         else
            springarea(1)=dm*4.d0
         endif
      endif
!
      if(int(elcon(3,1,imat)).eq.1) then
!
!        exponential overclosure
!
         if(dabs(elcon(2,1,imat)).lt.1.d-30) then
            elas(1)=0.d0
            beta=1.d0
         else
!     
            alpha=elcon(2,1,imat)*springarea(1)
            beta=elcon(1,1,imat)
            if(-beta*clear.gt.23.d0-dlog(alpha)) then
               beta=(dlog(alpha)-23.d0)/clear
            endif
            elas(1)=dexp(-beta*clear+dlog(alpha))
         endif
      elseif(int(elcon(3,1,imat)).eq.2) then
!     
!        linear overclosure
!     
         pi=4.d0*datan(1.d0)
         eps=-elcon(1,1,imat)*pi/elcon(2,1,imat)
         elas(1)=(-springarea(1)*elcon(2,1,imat)*clear*
     &            (0.5d0+datan(-clear/eps)/pi)) 
c     &	          -elcon(1,1,imat)*springarea(1)
      elseif(int(elcon(3,1,imat)).eq.3) then
!     
!        tabular overclosure
!
!        interpolating the material data
!
         call materialdata_sp(elcon,nelcon,imat,ntmat_,i,t1l,
     &     elconloc,kode,plicon,nplicon,npmat_,plconloc,ncmat_)
         overlap=-clear
         niso=int(plconloc(801))
         do i=1,niso
            xiso(i)=plconloc(2*i-1)
            yiso(i)=plconloc(2*i)
         enddo
         call ident(xiso,overlap,niso,id)
         if(id.eq.0) then
            pres=yiso(1)
         elseif(id.eq.niso) then
            pres=yiso(niso)
         else
            xk=(yiso(id+1)-yiso(id))/(xiso(id+1)-xiso(id))
            pres=yiso(id)+xk*(overlap-xiso(id))
         endif
         elas(1)=springarea(1)*pres
      endif
!
!     forces in the nodes of the contact element
!
      do i=1,3
         fnl(i,nope)=-elas(1)*xn(i)
      enddo
      if(iener.eq.1) then
         senergy=elas(1)/beta;
      endif
      cstr(4)=elas(1)/springarea(1)
!
!     Coulomb friction for static calculations
!
      if(ncmat_.ge.7) then
c         if((iperturb(1).gt.1).or.(nmethod.eq.4)) then
            um=elcon(6,1,imat)
            if(um.gt.0.d0) then
               if(1.d0 - dabs(xn(1)).lt.1.5231d-6) then       
!           
!     calculating the local directions on master surface
!
                  t1(1)=-xn(3)*xn(1)
                  t1(2)=-xn(3)*xn(2)
                  t1(3)=1.d0-xn(3)*xn(3)
               else
                  t1(1)=1.d0-xn(1)*xn(1)
                  t1(2)=-xn(1)*xn(2)
                  t1(3)=-xn(1)*xn(3)
               endif
               dt1=dsqrt(t1(1)*t1(1)+t1(2)*t1(2)+t1(3)*t1(3))
               do i=1,3
                  t1(i)=t1(i)/dt1
               enddo
               t2(1)=xn(2)*t1(3)-xn(3)*t1(2)
               t2(2)=xn(3)*t1(1)-xn(1)*t1(3)
               t2(3)=xn(1)*t1(2)-xn(2)*t1(1)           
!     
!              linear stiffness of the shear stress versus
!              slip curve
!
               xk=elcon(7,1,imat)*springarea(1)
!
!              calculating the relative displacement between the slave node
!              and its projection on the master surface
!
               do i=1,3
                  alnew(i)=vl(i,nope)
                  do j=1,nterms
                     alnew(i)=alnew(i)-ratio(j)*vl(i,j)
                  enddo
               enddo
!
!              calculating the difference in relative displacement since
!              the start of the increment = lamda^*
!
               do i=1,3
                  al(i)=alnew(i)-xstateini(3+i,1,ne0+konl(nope+1))
               enddo
!
!              ||lambda^*||
!
               val=al(1)*xn(1)+al(2)*xn(2)+al(3)*xn(3)
!
!              update the relative tangential displacement
!
               do i=1,3
                  t(i)=xstateini(6+i,1,ne0+konl(nope+1))+al(i)-val*xn(i)
               enddo
!
!              store the actual relative displacement and
!                    the actual relative tangential displacement
!
               do i=1,3
                  xstate(3+i,1,ne0+konl(nope+1))=alnew(i)
                  xstate(6+i,1,ne0+konl(nope+1))=t(i)
               enddo
!
!              size of normal force
!
               dfnl=dsqrt(fnl(1,nope)**2+fnl(2,nope)**2+fnl(3,nope)**2)
!
!              maximum size of shear force
!
               dfshear=um*dfnl       
!
!              plastic and elastic slip
!
               do i=1,3
                  tp(i)=xstateini(i,1,ne0+konl(nope+1))
                  te(i)=t(i)-tp(i)
               enddo
               dte=dsqrt(te(1)*te(1)+te(2)*te(2)+te(3)*te(3))
!
!              trial force
!
               do i=1,3
                  ftrial(i)=xk*te(i)
               enddo
               dftrial=dsqrt(ftrial(1)**2+ftrial(2)**2+ftrial(3)**2)
!
!              check whether stick or slip
!
               if((dftrial.lt.dfshear).or.(dftrial.le.0.d0).or.
     &            (ielas.eq.1)) then
!
!                 stick
!
c                  write(*,*)'STICK'
                  do i=1,3
                     fnl(i,nope)=fnl(i,nope)+ftrial(i)
                  enddo
                  cstr(5)=(ftrial(1)*t1(1)+ftrial(2)*t1(2)+
     &                    ftrial(3)*t1(3))/springarea(1)
                  cstr(6)=(ftrial(1)*t2(1)+ftrial(2)*t2(2)+
     &                    ftrial(3)*t2(3))/springarea(1)
               else
!
!                 slip
!
c                  write(*,*)'SLIP'
                  dg=(dftrial-dfshear)/xk
                  do i=1,3
                     ftrial(i)=te(i)/dte
                     fnl(i,nope)=fnl(i,nope)+dfshear*ftrial(i)
                     xstate(i,1,ne0+konl(nope+1))=tp(i)+dg*ftrial(i)
                  enddo
                  cstr(5)=(dfshear*ftrial(1)*t1(1)+
     &                    dfshear*ftrial(2)*t1(2)+
     &                    dfshear*ftrial(3)*t1(3))/springarea(1)
                  cstr(6)=(dfshear*ftrial(1)*t2(1)+
     &                    dfshear*ftrial(2)*t2(2)+
     &                    dfshear*ftrial(3)*t2(3))/springarea(1)

               endif
            endif
!
!     storing the tangential displacements
!     
            cstr(2)=t(1)*t1(1)+t(2)*t1(2)+t(3)*t1(3)
            cstr(3)=t(1)*t2(1)+t(2)*t2(2)+t(3)*t2(3)
c         endif
      endif
!
!     force in the master nodes
!
      do i=1,3
         do j=1,nterms
            fnl(i,j)=-ratio(j)*fnl(i,nope)
         enddo
      enddo
!
      return
      end

