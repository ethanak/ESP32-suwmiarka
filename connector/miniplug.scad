
// download spc_connector.stl from https://www.thingiverse.com/thing:3141366
module miniwtyczka()
{
    difference() {
        import("spc_connector.stl", convexity=10);
        translate([5,-10,4.1]) cube(20);
        translate([7,-10,4.1]) rotate([0,30,0]) cube(20);
        translate([-12,-10,-1]) cube([10,20,20]);
    }
}

miniwtyczka();
