#!/usr/bin/perl

use strict 'subs';
use Gimp;
use Gimp::Fu;

#
# this is quite convoluted, but I found no other way to do this than:
#
# create a new image & one layer
# copy & paste the layer
# ditherize new image
# copy & paste back
#

#Gimp::set_trace(TRACE_ALL);

my %imagetype2layertype = (
   RGB,		RGB_IMAGE,
   GRAY,	GRAY_IMAGE,
   INDEXED,	INDEXED_IMAGE,
);

register "plug_in_ditherize",
         "dithers current selection",
         "This script takes the current selection and dithers it just like convert to indexed",
         "Marc Lehmann",
         "Marc Lehmann",
         "1.2",
         __"<Image>/Filters/Noise/Ditherize",
         "RGB*, GRAY*",
         [
          [PF_RADIO,		"dither_type",	"The dither type (see gimp_convert_indexed)", 1,
          					[none => 0, fs => 1, "fs/low-bleed" => 2, ordered => 3]],
          [PF_SLIDER,		"colours",	"The number of colours to dither to", 10, [0, 256, 1, 1]],
         ],
         sub {
   my($image,$drawable,$dither,$colours)=@_;

   $drawable->is_layer or die "this plug-in only works for layers";

   $image->undo_push_group_start;

   # make sure something is selected
   $drawable->mask_bounds or $image->selection_all;

   my ($x1,$y1,$x2,$y2)=($drawable->mask_bounds)[1..4];
   my ($w,$h)=($x2-$x1,$y2-$y1);

   my $sel = $image->selection_save;
   $image->rect_select($x1,$y1,$w,$h,REPLACE,0,0);
   $drawable->edit_copy;
   $sel->selection_load;
   $sel->remove_channel;

   my $copy = new Image($w, $h, $image->base_type);
   $copy->undo_disable;
   my $draw = new Layer($copy, $w, $h,
                        $imagetype2layertype{$image->base_type},
                        "temporary layer", 100, NORMAL_MODE);
   $copy->add_layer ($draw, 1);
   $draw->edit_paste(0)->anchor;
   $copy->convert_indexed ($dither, MAKE_PALETTE, $colours, 1, 1, "");

   $draw->edit_copy;
   $drawable->edit_paste(1)->anchor;
   $copy->delete;

   $image->undo_push_group_end;

   ();
};

exit main;

