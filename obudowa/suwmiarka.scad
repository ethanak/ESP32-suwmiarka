
// Co drukujemy?
toprint = 0; // [0:obudowa, 1: przykrywka, 2: przekładka, 3: uchwyt głośnika.,4: złożenie]
// rozmiar głośnika
spkrh = 0; // [0: płaski, 1: wysoki]


module __Customizer_Limit__ () {}
$fn=32;

basew = 29.5;
baseh = 13.3;
basel = 75;

baseg = 1.3;
lpad = 27.5;
padlen = 25.5;
padh=8;

dpipx=34;
dpipl=7;
pipg=2;
upipx=[19,49, 19+42.5];

haku=5.4;

hgr=baseg+6+1+haku+2+1.2;
pipek=6.4;
xpipek=basel-pipek/2-10;
xkabel=50;

padx=[8.5, 47];
pady=[basew/2+10.5,basew/2-10.5];

module pipak(adl=0)
{
    h=baseg+hgr+14.4+adl;
    g=1.5;
    cube([dpipl-0.4,h,g]);
    translate([0,h-2,0]) cube([dpipl-0.4,2,g+2.4]);
    translate([-1.2,0,0]) {
        cube([dpipl+2.0,g+1,0.8]);
        cube([dpipl+2,hgr-5,g]);
        cube([dpipl+2,0.8,g+1]);
    }
    
}

module pipek(n=0)

{
                h=(n)?baseh+baseg-1:baseh+baseg;
                cube([dpipl-0.4,1.1,h+pipg+hgr+1]);
    echo("PIP",h+pipg+hgr+1,baseg+hgr+14.4);
    
                translate([0,0,h+hgr+1]) cube([dpipl-0.4,7,pipg]);
                translate([-1.2,0,0]) cube([dpipl+2,1.1,hgr-5]);
                translate([-1.2,0,0]) cube([dpipl+2,1.1+1.6,1.1]);

}
module obudowa(pan=3,hgl=1,grx=0)
{
    difference() {
        union() {
            translate([0,-1.6,0-grx]) cube([basel, basew+3.2, baseg+grx]);
            for (y=[-1.6,basew]) translate([0,y,0]) cube([basel,1.6,hgr+baseg]);
            for (y=[0,basew-4]) {
                translate([basel-5,y,0]) cube([5,4,hgr]);
           *     translate([0,y,baseg+7.2]) cube([5,5,hgr-7.2-baseg]);
            }
            translate([0,-1.6,0]) cube([1.6,1.6+1.4,6+baseg]);
            translate([0,10.5,0]) difference() {
                cube([1.6,basew-10,6+baseg]);
                translate([-1,16.5-10.5,2+baseg]) cube([2.7,basew-16.5-2,10]);
            }
            for (x=[/*0,*/basel-1.6]) {
                h=x?0:6+baseg;
                translate([x,-1.6,h]) cube([1.6,basew+3.2,hgr-h]);
            }
            for (i=[0,1]) translate([padx[i],pady[i],0]) cylinder(d=5,h=baseg+6);
            // wył
         *  translate([0,basew/2-8,hgr-5]) cube([4.8,16,5]); 
            // but
            translate([xpipek-7,basew-3.2,hgr-pipek-1]) hull() {
                cube([14,4.8,pipek+1]);
                translate([0,3.2,-3.2]) cube([14,1.6,1]);
            }
            // lucha
            translate([lpad-15,-1.6,hgr]) {
        //cube([padlen,2,baseg]);
        if (pan == 2 || pan == 3) {
            ex = (dpipx+dpipl-lpad+1) +
            (padlen+lpad-(dpipx+dpipl+1));
            cube([ex,1.6,padh+baseg]);
            *for (x=[0,ex-2]) translate([x,-2,-hgr]) cube([2,3,hgr+padh+baseg]);
            
        }
        else {
            cube([dpipx-lpad-1,1.6,padh+baseg]);
        //cube([dpipx-lpad-1,1.6,padh+baseg]);
            translate([dpipx+dpipl-lpad+1,0,0]) cube([padlen+lpad-(dpipx+dpipl+1),1.6,padh+baseg]);
            }
        }
                // ducha
        if (pan == 2 || pan == 3) {
        }
        else if (pan) {
            translate([dpipx-15,-2.4,0]) {
                cube([dpipl,2.4,baseh+baseg+pipg+hgr]);
                translate([0,0,baseh+baseg+hgr]) cube([dpipl,4,pipg]);
                translate([0,-0.8,baseh+baseg+hgr-2]) hull() {
                    translate([0,0,0.8]) cube([dpipl,1,pipg+1.2]);
                    translate([0,0.8,0])cube([dpipl,1,pipg+2]);
                
                }
            }
        }
        else {
            translate([dpipx-15,-3.2,0]) difference () {
                translate([-3.2,-1.6,0]) cube([dpipl+6.4,1.6+2.4,hgr]);
                    translate([-1.6,0,-1]) cube([dpipl+3.2,1.6,hgr+2]);
                    translate([0,-4,-1]) cube([dpipl,4.6,hgr+2]);
            }
        }
            // ucha
        if (pan == 2 || pan == 3) {
            translate([12.5,basew,hgr]) cube([11.5,1.6,padh+baseg]);
            *translate([12.5,basew,0]) cube([2,3.6,padh+baseg+hgr]);
            *translate([38,basew,0]) cube([2,3.6,padh+baseg+hgr]);
            translate([34,basew,hgr]) cube([7.9,1.6,padh+baseg]);
            x=upipx[2];
            if (pan == 2) translate([x-15,basew,0]) {
                difference() {
                    translate([-3.2,0,0]) cube([dpipl+6.4,1.6+4,hgr]);
                    translate([-1.6,1.6,-1]) cube([dpipl+3.2,1.6,hgr+2]);
                    translate([0,1.6,-1]) cube([dpipl,4.6,hgr+2]);
                }
            }
        }
        else if (pan) {
            for (x=upipx) translate([x-15,basew,0]) {
                h=(x>50)?baseh+baseg-1:baseh+baseg;
                u=(x>50)?1:2;
                cube([dpipl,2.4,h+pipg+hgr]);
                translate([0,-2.2,h+hgr]) cube([dpipl,5,pipg]);
                translate([0,0,baseh+baseg+hgr-2]) hull() {
                    translate([0,0,0.8]) cube([dpipl,3.2,pipg+u-0.8]);
                    cube([dpipl,2.4,pipg+u]);
                    }
                }
            }
        else {
            translate([12.5,basew,hgr]) cube([11.5,1.6,padh+baseg]);
            translate([34,basew,hgr]) cube([11.5,1.6,padh+baseg]);
            for (x=upipx) translate([x-15,basew,0]) {
                h=(x>50)?baseh+baseg-1:baseh+baseg;
                u=(x>50)?1:2;
                difference() {
                    translate([-3.2,0,0]) cube([dpipl+6.4,1.6+2.4,hgr]);
                    translate([-1.6,1.6,-1]) cube([dpipl+3.2,1.6,hgr+2]);
                    translate([0,1.6,-1]) cube([dpipl,4.6,hgr+2]);
                }
                //cube([dpipl,2.4,h+pipg+hgr]);
                }
            }
        }
        
        if (!pan) {
            for (x=upipx) translate([x-15,basew,-1]) {
                translate([-1.8,0,0]) cube([dpipl+3.6,1.6+2.4,2.1]);
            }
                translate([dpipx-15-1.8,-5.6,-1]) cube([dpipl+3.6,5.6+0.6,2.1]);
        }
        
        
        translate([xpipek-pipek/2,basew-4,hgr-pipek]) cube([pipek,14.8,pipek+3]);
        translate([xpipek-pipek/2-2.5,basew-1.6,hgr-pipek]) cube([pipek+5,1.6,hgr+1]);
        translate([xpipek-pipek/2-1,basew-4,hgr-pipek-6]) cube([pipek+2,4,14]);
        
        translate([xkabel-3-6,basew-1,hgr-7]) cube([5.5,16,26]);
        
        translate([basel-15.5,basew/2,-1]) {
            translate([0,0,-grx]) intersection() {
                cylinder(d=24,h=10);
                for (y=[-15:2:15]) translate([-15,y,0]) cube([30,0.8,10]);
                *for (x=[-5:5]) for (y=[-5:5]) translate([3*x,3*y,0]) cylinder(d=2.4,h=10);
                }
            translate([0,0,1+0.7]) cylinder(d=27.4,h=1.5);
        }
        for (x=[2.4, basew-2.4]) translate([basel-3,x,0]) {
            translate([0,0,2]) cylinder(d=1.8, h=20);
         
        }
        for (i=[0,1]) translate([padx[i],pady[i],2]) cylinder(d=1.8,h=baseg+6);
        
        for (z=[1+5+2.5+1.6]) for (y=[basew/2-7.5,basew/2+7.5]) translate([basel-2,y,z+(hgl?2.1:0)]) rotate([0,90,0]) {
            cylinder(d=2.4,h=10);
            translate([0,0,1.4]) cylinder(d1=2.4, d2=4.4,h=1.01);
        }
        
        // ucha
        
        
            if (pan == 2) {
                translate([upipx[2]-15,basew,0]) {
                    //translate([-3.2,0,0]) cube([dpipl+6.4,1.6+4,hgr]);
                    translate([-1.6,0.8,-1]) cube([dpipl+3.2,2.4,hgr+20]);
                    translate([0,1.6,-1]) cube([dpipl,4.6,hgr+20]);
                    translate([-1.6,-1.6,-1]) cube([dpipl+3.2,5,2.1]);
                }
            

            }
            else if (pan == 1) {
                for (x=upipx) translate([x-15.8,basew-1,hgr-4]) {
                    cube([0.8,4,6]);
                    translate([dpipl+0.8,0,0]) cube([0.8,4,6]);
            }
            translate([dpipx-15-0.8,-2.6,hgr-4]) {
        
                cube([0.8,4,6]);
                translate([dpipl+0.8,0,0]) cube([0.8,4,6]);
            }
       }
   }
}

module przekladka()
{
    hon = 6+baseg+1.2;
    hga = hgr - hon;
    difference()
    {
        for (y=[0.4,basew-5.4]) {
                
                translate([0,y,baseg+7.2+1.6]) cube([5,5,hgr-7.2-baseg-1.6]);
            }
    for (y=[0.4+3,basew-0.4-3]) translate([3,y,0]) {
            translate([0,0,2]) cylinder(d=1.8, h=20);
         
        }
    }
    
    translate([0,basew/2,hon]) difference()
    {
    union() {
        translate([0,-10,hgr-5-hon]) cube([4.8,20,5]); 
        
        
        translate([0,-basew/2+0.4,0]) cube([1.6,basew-0.8,hga]);
        for (x=[8.5, 47]) for (y=[-10.5,10.5]) translate([x,y,0]) cylinder(d=5,h=1.1+1.6);
        
        for (x=[8.5, 47]) hull() {
            for (y=[-10.5,10.5]) translate([x,y,1.6]) cylinder(d=5,h=1.1);
            }
        translate([0,-8,1.6]) cube([47,16,1.1]);
        }
        translate([-1,-4.8,hgr-5.01-hon]) cube([8,9.6,6]); 
        translate([1.6,-8,hgr-5-hon]) cube([1.6,16,5]);
        
        
        for (x=[8.5, 47]) for (y=[-10.5,10.5]) translate([x,y,-1]) {
            cylinder(d=2.4,h=10);
            translate([0,0,2.7]) cylinder(d2=4.4, d1=2.4,h=1.1);
        }
    }
    
    
}

module podstawa(pan=0)
{
    translate([0,0,hgr+0.1]) {
        difference() {
            union() {
                
                translate([xpipek-pipek/2+0.4,basew-1,0]) cube([pipek-0.8,2.6,baseg]);
                translate([0,0.4,0]) cube([basel,basew-0.8,baseg]);
                if (pan == 3) {
                    translate([xkabel-3-6+0.4,basew,-hgr+12]) cube([5.5-0.8,1.6,hgr+baseg-12]);
                    translate([xkabel-3-6+0.4,basew-1,0]) cube([5.5-0.8,2.6,baseg]);
                    
                }
            }
            for (y=[0.4+3,basew-0.4-3]) translate([3,y,-1]) {
            cylinder(d=2.4, h=20);
            translate([0,0,1.6]) cylinder(d1=2.4,d2=4.4,h=1.01);
         
        }
        
        for (y=[2.4,basew-2.4]) translate([basel-3,y,-1]) {
            cylinder(d=2.4, h=20);
            translate([0,0,1.6]) cylinder(d1=2.4,d2=4.4,h=1.01);
         
        }
        
        
        }
    }
}

module spiker()
{
    espkr=10.5;
    z=1+5+2.5+1.6;
    ez=hgr-z;
    dz=espkr-ez;    

    module symetric() {
        for (i=[0,1]) mirror([0,i,0]) children();
    }

    difference() {
        union() {
            translate([0,-7,0]) cube([15,14,1.7]);
            translate([0,-10,0]) cube([2.0,20,dz+4]);
            //new
            translate([5,-11,0]) cube([15,25,1.7]);
            for (x=[-3.5,4.5])  translate([10,x*2.54,0]) cylinder(d=4,h=4);
            symetric() translate([0,7.5,dz]) rotate([0,90,0]) cylinder(d=4.4,h=3.2);
        }
        symetric() translate([-1,7.5,dz]) rotate([0,90,0]) cylinder(d=1.8,h=7);
        for (x=[-3.5,4.5])  translate([10,x*2.54,-1]) cylinder(d=1.8,h=7);
    }
        

}

module lolin()
{
    w=basew-2;
    color([0.5,0.4,0.2]) difference() {
        translate([0,-w/2,0]) cube([51,w,1.6]);
        for (xy=[[8.5,10.5], [47,-10.5]]) translate([xy[0], xy[1],-1]) cylinder(d=2.4,h=5);
            for (y=[-10,10]) for (x=[0:12]) translate([12+2.54*x,y,-1]) cylinder(d=1,h=3);
        }
    color("white") translate([0,-13.2,-5.6]) difference() {
        cube([4,9,5.6]);
        translate([-1,0.5,0.5]) cube([4,8,4.6]);
        }
    color("white") translate([0.6,3,-2]) cube([5,8,2]);
        
}

module rspeaker()
{
    translate([basel-15.5,basew/2,0.7]) {
        if (!spkrh) {
            hull() {
                cylinder(d=27, h=1);
                cylinder(d=15,h=3);
            }
            
            cylinder(d=15,h=5.9);
        }
        else {
            hull(){
                cylinder(d=27,h=1);
                cylinder(d=12,h=3);
            }
            cylinder(d=12,h=6.6);
            translate([0,0,4]) cylinder(d=15,h=3.6);    
            
        }
    }
        
}


module zlozenie()
{
    %obudowa(3,spkrh,0.6);
    color("gray") rspeaker();
    translate([0,basew/2,7]) lolin();
    translate([0,0,20]) color("green") przekladka();
    translate([0,0,40]) podstawa(3);
    color("red")translate([74,14.7,spkrh?8.6:6.6])rotate([0,0,180]) spiker();
}

if (toprint == 0) obudowa(3,spkrh, 0.6);
else if (toprint == 1) podstawa(3);
else if (toprint == 2) przekladka();
else if (toprint == 3) spiker();
else zlozenie();

//obudowa(3,1,0.6);
//pipak(0.8);
//pipek(1);
//przekladka();
//podstawa(3);
//spiker();
 //color("red")translate([74,14.7,8.6])rotate([0,0,180]) spiker();