#!@THEGIMPTCL@
# -*-Mode: Tcl;-*-
##################################################
# file: pdb-help.tcl
#
# Copyright (c) 1997 Eric L. Hernes (erich@rrnet.com)
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. The name of the author may not be used to endorse or promote products
#    derived from this software withough specific prior written permission
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $Id$
#

proc gimptcl_query {} {
    gimp-install-procedure "pdb_help" \
	"Interactive Help with the Procedural Database" \
	"Pick a PDB proc; then press `Query' for information" \
	"Eric L. Hernes" \
	"Eric L. Hernes" \
	"1997" \
        "<Toolbox>/Xtns/PDB Help" \
	"" \
	extension \
	{
	    {int32 "run_mode" "Interactive, non-interactive"}
	} \
	{}
}

proc gimptcl_run {mode} {
    pdb-help-init
}

proc show_proc {p} {
    global name author copyright

    set descr [gimp-query-dbproc $p]
    set name [lindex $descr 0]
    set blurb [lindex $descr 1]
    .about.descr.blurb delete 0.0 end
    .about.descr.blurb insert 0.0 $blurb

    set help [lindex $descr 2]
    set author [lindex $descr 3]
    set c [lindex $descr 4]
    set date [lindex $descr 5]
    set type [lindex $descr 6]
    set args [lindex $descr 7]
    set retvals [lindex $descr 8]

    set copyright "Copyright $c, $date"
    .about.descr.ar.a.t.t delete 0.0 end
    if {[llength $args] == 0} {
	.about.descr.ar.a.t.t insert 0.0 "None"
    } else {
	set arg_string ""
	foreach a $args {
	    set t [lindex $a 0]
	    set n [lindex $a 1]
	    set v [lindex $a 2]
	    set arg_string "$arg_string$n ($t)\n"
	    set arg_string "$arg_string    $v\n\n"
	}
	.about.descr.ar.a.t.t insert 0.0 $arg_string
    }

    .about.descr.ar.r.t.t delete 0.0 end
    if {[llength $retvals] == 0} {
	.about.descr.ar.r.t.t insert 0.0 "nothing"
    } else {
	set ret_string ""
	foreach r $retvals {
	    set t [lindex $r 0]
	    set n [lindex $r 1]
	    set v [lindex $r 2]
	    set ret_string "$ret_string$n ($t)\n"
	    set ret_string "$ret_string    $v\n\n"
	}
	.about.descr.ar.r.t.t insert 0.0 $ret_string
    }

    .about.descr.help delete 0.0 end
    .about.descr.help insert 0.0 $help
}

proc run_proc {p} {
    show_proc $p

    if {[winfo exists .args]} {
	destroy .args
    }

    toplevel .args
    frame .args.entry -relief ridge -border 3
    frame .args.ctl -relief ridge -border 3

    set descr [gimp-query-dbproc $p]
    set name [lindex $descr 0]

    label .args.title -relief ridge -border 3 -text $name
    wm title .args "PDB run: $name"

    set args [lindex $descr 7]
    set retvals [lindex $descr 8]
    set na [llength $args]

    for {set a 0} {$a < $na} {incr a} {
	set thisArg [lindex $args $a]
	set n [lindex $thisArg 1]
	label .args.entry.l$a -text $n
	entry .args.entry.e$a -width 10

	grid .args.entry.l$a -row $a -column 0
	grid .args.entry.e$a -row $a -column 1
    }
#    pack .args.entry.l .args.entry.e -side left
    button .args.ctl.ok -text "Ok" -command {
	set p [.args.title cget -text]
	set pdb "gimp-run-procedure $p"
	puts "running $p"

	set descr [gimp-query-dbproc $p]
	set args [lindex $descr 7]
	set retvals [lindex $descr 8]
	set na [llength $args]
	set nr [llength $retvals]

	for {set a 0} {$a < $na} {incr a} {
	    append pdb " [.args.entry.e$a get]"
	}
	set rets [eval $pdb]

	if {[winfo exists .rets]} {
	    destroy .rets
	}

	toplevel .rets
	
	frame .rets.rets -relief ridge -border 3
	frame .rets.ctl -relief ridge -border 3

	label .rets.title -relief ridge -border 3 -text "$name results"
	wm title .rets "PDB results: $name"

	for {set r 0} {$r < $nr} {incr r} {
	    set thisRet [lindex $retvals $r]
	    set n [lindex $thisRet 1]
	    label .rets.rets.l$r -text $n
	    label .rets.rets.v$r -text [lindex $rets $r]

	    grid .rets.rets.l$r -row $r -column 0
	    grid .rets.rets.v$r -row $r -column 1
	}
	button .rets.ctl.ok -text "Ok" -command {destroy .rets}

	pack .rets.ctl.ok -side left
	pack .rets.title -fill both
	pack .rets.rets .rets.ctl
    }

    button .args.ctl.cancel -text "Cancel" -command {destroy .args}

    pack .args.ctl.cancel .args.ctl.ok -side left
    pack .args.title -fill both
    pack .args.entry .args.ctl
}

proc pdb-help-init {} {
    wm title . "PDB Help"
    set db_procs [gimp-query-db .* .* .* .* .* .* .*]

    frame .about
    frame .about.procs
    scrollbar .about.procs.sb -width 12 -command ".about.procs.list yview"
    listbox .about.procs.list -height 20 -width 30 -yscroll ".about.procs.sb set"
    bind .about.procs.list <Double-Button-1> "run_proc \[selection get\]"
    bind .about.procs.list <ButtonRelease-1> {
	tkCancelRepeat
	%W activate @%x,%y
	show_proc [selection get]
    }

    foreach p [lsort $db_procs] {
	.about.procs.list insert end $p
    }
    pack .about.procs.list .about.procs.sb -side left -expand 1 -fill both
    
    frame .about.descr
    label .about.descr.name -textvariable name \
	-font -*-Helvetica-Bold-R-Normal--*-180-*-*-*-*-*-*
    label .about.descr.author -justify left -width 40 -textvariable author \
	-font -*-Helvetica-medium-r-*-*-*-120-*-*-*-*-*-* \
	-wraplength 320 -height 2
    label .about.descr.copyright -textvariable copyright \
	-font -*-Helvetica-medium-r-*-*-*-80-*-*-*-*-*-*

    frame .about.descr.b
    text .about.descr.blurb -relief ridge -borderwidth 3 -wrap word \
	-width 40 -height 2 \
	-font -*-Helvetica-medium-r-*-*-*-120-*-*-*-*-*-* \
	-yscrollcommand ".about.descr.b.sb set"
    scrollbar .about.descr.b.sb -width 6 \
	-command ".about.descr.blurb yview"
    pack .about.descr.blurb -in .about.descr.b -side left -fill both
    pack .about.descr.b.sb -side left -fill both
    
    frame .about.descr.h
    text .about.descr.help -relief ridge -borderwidth 3 -wrap word \
	-width 40 -height 7 \
	-font -*-Helvetica-medium-r-*-*-*-120-*-*-*-*-*-* \
	-yscrollcommand ".about.descr.h.sb set"
    scrollbar .about.descr.h.sb -width 6 \
	-command ".about.descr.help yview"
    pack .about.descr.help -in .about.descr.h -side left -fill both
    pack .about.descr.h.sb -side left -fill both
    
    frame .about.descr.ar
    
    frame .about.descr.ar.a -relief ridge -borderwidth 3
    label .about.descr.ar.a.l -text "Arguments:"
    frame .about.descr.ar.a.t
    text .about.descr.ar.a.t.t -relief sunken -borderwidth 3 -wrap word \
	-width 40 -height 7 \
	-font -*-Helvetica-medium-r-*-*-*-120-*-*-*-*-*-* \
	-yscrollcommand ".about.descr.ar.a.t.sb set"
    scrollbar .about.descr.ar.a.t.sb -width 6 \
	-command ".about.descr.ar.a.t.t yview"
    pack .about.descr.ar.a.t.t .about.descr.ar.a.t.sb -side left -fill both
    pack .about.descr.ar.a.l .about.descr.ar.a.t -side top -anchor w
    
    frame .about.descr.ar.r -relief ridge -borderwidth 3
    label .about.descr.ar.r.l -text "Returns:"
    frame .about.descr.ar.r.t
    text .about.descr.ar.r.t.t -relief sunken -borderwidth 3 -wrap word \
	-width 40 -height 7 \
	-font -*-Helvetica-medium-r-*-*-*-120-*-*-*-*-*-* \
	-yscrollcommand ".about.descr.ar.r.t.sb set"
    scrollbar .about.descr.ar.r.t.sb -width 6 \
	-command ".about.descr.ar.r.t.t yview"
    pack .about.descr.ar.r.t.t .about.descr.ar.r.t.sb -side left -fill both
    pack .about.descr.ar.r.l .about.descr.ar.r.t -side top -anchor w

    pack .about.descr.ar.a .about.descr.ar.r -side top

    pack .about.descr.name .about.descr.author .about.descr.copyright \
	.about.descr.b .about.descr.h .about.descr.ar -side top -anchor w
    
    pack .about.procs .about.descr -side left -anchor nw -expand 1 -fill both

    frame .ctl
    button .ctl.b -text "Bye" -command {destroy .}
    button .ctl.query -text "Query" -command {show_proc [selection get]}
    button .ctl.run -text "Run" -command {run_proc [selection get]}
    pack .ctl.b .ctl.query .ctl.run -side left -fill both -expand 1
    pack .about .ctl

    show_proc "pdb_help"
}

if {![string compare [interp slave] ""]} {
	uplevel \#0 gimptcl_run 1
}
