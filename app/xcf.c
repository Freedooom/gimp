#include "config.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "cursorutil.h"
#include "floating_sel.h"
#include "gimage.h"
#include "gimage_mask.h"
#include "interface.h"
#include "plug_in.h"
#include "procedural_db.h"
/* #include "tile_swap.h"*/
#include "xcf.h"

#include "libgimp/gimpintl.h"

#include "drawable_pvt.h"
#include "layer_pvt.h"
#include "channel_pvt.h"
#include "tile_manager_pvt.h"
#include "tile.h"			/* ick. */
#include <libgimp/parasiteP.h>
#include <libgimp/parasite.h>
#include "parasitelist.h"
#include "pathsP.h"
#include <libgimp/gimpunit.h>
#include <libgimp/gimplimits.h>

/* #define SWAP_FROM_FILE */


typedef enum
{
  PROP_END = 0,
  PROP_COLORMAP = 1,
  PROP_ACTIVE_LAYER = 2,
  PROP_ACTIVE_CHANNEL = 3,
  PROP_SELECTION = 4,
  PROP_FLOATING_SELECTION = 5,
  PROP_OPACITY = 6,
  PROP_MODE = 7,
  PROP_VISIBLE = 8,
  PROP_LINKED = 9,
  PROP_PRESERVE_TRANSPARENCY = 10,
  PROP_APPLY_MASK = 11,
  PROP_EDIT_MASK = 12,
  PROP_SHOW_MASK = 13,
  PROP_SHOW_MASKED = 14,
  PROP_OFFSETS = 15,
  PROP_COLOR = 16,
  PROP_COMPRESSION = 17,
  PROP_GUIDES = 18,
  PROP_RESOLUTION = 19,
  PROP_TATTOO = 20,
  PROP_PARASITES = 21,
  PROP_UNIT = 22,
  PROP_PATHS = 23,
  PROP_USER_UNIT = 24
} PropType;

typedef enum
{
  COMPRESS_NONE = 0,
  COMPRESS_RLE = 1,
  COMPRESS_ZLIB = 2,
  COMPRESS_FRACTAL = 3  /* Unused. */
} CompressionType;

typedef GImage* XcfLoader(XcfInfo *info);

static Argument* xcf_load_invoker (Argument  *args);
static Argument* xcf_save_invoker (Argument  *args);

static gint xcf_save_image         (XcfInfo     *info,
				    GImage      *gimage);
static void xcf_save_choose_format (XcfInfo     *info,
				    GImage      *gimage);
static void xcf_save_image_props   (XcfInfo     *info,
				    GImage      *gimage);
static void xcf_save_layer_props   (XcfInfo     *info,
				    GImage      *gimage,
				    Layer       *layer);
static void xcf_save_channel_props (XcfInfo     *info,
				    GImage      *gimage,
				    Channel     *channel);
static void xcf_save_prop          (XcfInfo     *info,
				    PropType     prop_type,
				    ...);
static void xcf_save_layer         (XcfInfo     *info,
				    GImage      *gimage,
				    Layer       *layer);
static void xcf_save_channel       (XcfInfo     *info,
				    GImage      *gimage,
				    Channel     *channel);
static void xcf_save_hierarchy     (XcfInfo     *info,
				    TileManager *tiles);
static void xcf_save_level         (XcfInfo     *info,
				    TileManager *tiles);
static void xcf_save_tile          (XcfInfo     *info,
				    Tile        *tile);
static void xcf_save_tile_rle      (XcfInfo     *info,
				    Tile        *tile,
				    guchar      *rlebuf);

static GImage*  xcf_load_image         (XcfInfo     *info);
static gint     xcf_load_image_props   (XcfInfo     *info,
					GImage      *gimage);
static gint     xcf_load_layer_props   (XcfInfo     *info,
					GImage      *gimage,
					Layer       *layer);
static gint     xcf_load_channel_props (XcfInfo     *info,
					GImage      *gimage,
					Channel     *channel);
static gint     xcf_load_prop          (XcfInfo     *info,
					PropType    *prop_type,
					guint32     *prop_size);
static Layer*   xcf_load_layer         (XcfInfo     *info,
					GImage      *gimage);
static Channel* xcf_load_channel       (XcfInfo     *info,
					GImage      *gimage);
static LayerMask* xcf_load_layer_mask  (XcfInfo     *info,
					GImage      *gimage);
static gint     xcf_load_hierarchy     (XcfInfo     *info,
					TileManager *tiles);
static gint     xcf_load_level         (XcfInfo     *info,
					TileManager *tiles);
static gint     xcf_load_tile          (XcfInfo     *info,
					Tile        *tile);
static gint     xcf_load_tile_rle      (XcfInfo     *info,
					Tile        *tile,
					int         data_length);

#ifdef SWAP_FROM_FILE
static int      xcf_swap_func          (int          fd,
					Tile        *tile,
					int          cmd,
					gpointer     user_data);
#endif


static void xcf_seek_pos (XcfInfo *info,
			  guint    pos);
static void xcf_seek_end (XcfInfo *info);

static guint xcf_read_int32   (FILE     *fp,
			       guint32  *data,
			       gint      count);
static guint xcf_read_float   (FILE     *fp,
			       gfloat   *data,
			       gint      count);
static guint xcf_read_int8    (FILE     *fp,
			       guint8   *data,
			       gint      count);
static guint xcf_read_string  (FILE     *fp,
			       gchar   **data,
			       gint      count);
static guint xcf_write_int32  (FILE     *fp,
			       guint32  *data,
			       gint      count);
static guint xcf_write_float  (FILE     *fp,
			       gfloat   *data,
			       gint      count);
static guint xcf_write_int8   (FILE     *fp,
			       guint8   *data,
			       gint      count);
static guint xcf_write_string (FILE     *fp,
			       gchar   **data,
			       gint      count);



static ProcArg xcf_load_args[] =
{
  { PDB_INT32,
    "dummy_param",
    "dummy parameter" },
  { PDB_STRING,
    "filename",
    "The name of the file to load" },
  { PDB_STRING,
    "raw_filename",
    "The name of the file to load" },
};

static ProcArg xcf_load_return_vals[] =
{
  { PDB_IMAGE,
    "image",
    "Output image" },
};

static PlugInProcDef xcf_plug_in_load_proc =
{
  "gimp_xcf_load",
  "<Load>/XCF",
  NULL,
  "xcf",
  "",
  "0,string,gimp\\040xcf\\040",
  NULL, /* ignored for load */
  0,    /* ignored for load */
  {
    "gimp_xcf_load",
    "loads file saved in the .xcf file format",
    "The xcf file format has been designed specifically for loading and saving tiled and layered images in the GIMP. This procedure will load the specified file.",
    "Spencer Kimball & Peter Mattis",
    "Spencer Kimball & Peter Mattis",
    "1995-1996",
    PDB_INTERNAL,
    3,
    xcf_load_args,
    1,
    xcf_load_return_vals,
    { { xcf_load_invoker } },
  },
  NULL, /* fill me in at runtime */
  NULL /* fill me in at runtime */
};

static ProcArg xcf_save_args[] =
{
  { PDB_INT32,
    "dummy_param",
    "dummy parameter" },
  { PDB_IMAGE,
    "image",
    "Input image" },
  { PDB_DRAWABLE,
    "drawable",
    "Active drawable of input image" },
  { PDB_STRING,
    "filename",
    "The name of the file to save the image in" },
  { PDB_STRING,
    "raw_filename",
    "The name of the file to load" },
};

static PlugInProcDef xcf_plug_in_save_proc =
{
  "gimp_xcf_save",
  "<Save>/XCF",
  NULL,
  "xcf",
  "",
  NULL,
  "RGB*, GRAY*, INDEXED*",
  0, /* fill me in at runtime */
  {
    "gimp_xcf_save",
    "saves file in the .xcf file format",
    "The xcf file format has been designed specifically for loading and saving tiled and layered images in the GIMP. This procedure will save the specified image in the xcf file format.",
    "Spencer Kimball & Peter Mattis",
    "Spencer Kimball & Peter Mattis",
    "1995-1996",
    PDB_INTERNAL,
    5,
    xcf_save_args,
    0,
    NULL,
    { { xcf_save_invoker } },
  },
  NULL, /* fill me in at runtime */
  NULL /* fill me in at runtime */
};

static XcfLoader* xcf_loaders[] =
{
  xcf_load_image,			/* version 0 */
  xcf_load_image			/* version 1 */
};

static int N_xcf_loaders=(sizeof(xcf_loaders)/sizeof(xcf_loaders[0]));

void
xcf_init ()
{
  /* So this is sort of a hack, but its better than it was before.  To do this
   * right there would be a file load-save handler type and the whole interface
   * would change but there isn't, and currently the plug-in structure contains
   * all the load-save info, so it makes sense to use that for the XCF load/save
   * handlers, even though they are internal.  The only thing it requires is
   * using a PlugInProcDef struct.  -josh */
  procedural_db_register (&xcf_plug_in_save_proc.db_info);
  procedural_db_register (&xcf_plug_in_load_proc.db_info);
  xcf_plug_in_save_proc.image_types_val = plug_in_image_types_parse (xcf_plug_in_save_proc.image_types);
  xcf_plug_in_load_proc.image_types_val = plug_in_image_types_parse (xcf_plug_in_load_proc.image_types);
  plug_in_add_internal (&xcf_plug_in_save_proc);
  plug_in_add_internal (&xcf_plug_in_load_proc);
}

static Argument*
xcf_load_invoker (Argument *args)
{
  XcfInfo info;
  Argument *return_args;
  GImage *gimage;
  char *filename;
  int success;
  char id[14];

  gimp_add_busy_cursors();

  gimage = NULL;

  success = FALSE;

  filename = args[1].value.pdb_pointer;

  info.fp = fopen (filename, "rb");
  if (info.fp)
    {
      info.cp = 0;
      info.filename = filename;
      info.active_layer = 0;
      info.active_channel = 0;
      info.floating_sel_drawable = NULL;
      info.floating_sel = NULL;
      info.floating_sel_offset = 0;
      info.swap_num = 0;
      info.ref_count = NULL;
      info.compression = COMPRESS_NONE;

      success = TRUE;
      info.cp += xcf_read_int8 (info.fp, (guint8*) id, 14);
      if (strncmp (id, "gimp xcf ", 9) != 0)
	success = FALSE;
      else if (strcmp (id+9, "file") == 0) 
	{
	  info.file_version = 0;
	} 
      else if (id[9] == 'v') 
	{
	  info.file_version = atoi(id + 10);
	}
      else 
	success = FALSE;
      
      if (success)
	{
	  if (info.file_version < N_xcf_loaders)
	    {
	      gimage = (*(xcf_loaders[info.file_version])) (&info);
	      if (!gimage)
		success = FALSE;
	    }
	  else 
	    {
	      g_message (_("XCF error: unsupported XCF file version %d encountered"), info.file_version);
	      success = FALSE;
	    }
	}
      fclose (info.fp);
    }
  
  return_args = procedural_db_return_args (&xcf_plug_in_load_proc.db_info, success);

  if (success)
    return_args[1].value.pdb_int = pdb_image_to_id(gimage);

  gimp_remove_busy_cursors(NULL);

  return return_args;
}

static Argument*
xcf_save_invoker (Argument *args)
{
  XcfInfo info;
  Argument *return_args;
  GImage *gimage;
  char *filename;
  int success;

  gimp_add_busy_cursors();

  success = FALSE;

  gimage = gimage_get_ID (args[1].value.pdb_int);
  filename = args[3].value.pdb_pointer;

  info.fp = fopen (filename, "wb");
  if (info.fp)
    {
      info.cp = 0;
      info.filename = filename;
      info.active_layer = 0;
      info.active_channel = 0;
      info.floating_sel_drawable = NULL;
      info.floating_sel = NULL;
      info.floating_sel_offset = 0;
      info.swap_num = 0;
      info.ref_count = NULL;
      info.compression = COMPRESS_RLE;

      xcf_save_choose_format (&info, gimage);

      success = xcf_save_image (&info, gimage);
      fclose (info.fp);
    }
  else
    {
      g_message (_("open failed on %s: %s\n"), filename, g_strerror(errno));
    }

  return_args = procedural_db_return_args (&xcf_plug_in_save_proc.db_info, success);

  gimp_remove_busy_cursors(NULL);

  return return_args;
}

static void
xcf_save_choose_format (XcfInfo *info,
			GImage  *gimage)
{
  int save_version = 0;			/* default to oldest */

  if (gimage->cmap) 
    save_version = 1;			/* need version 1 for colormaps */

  info->file_version = save_version;
}

static gint
xcf_save_image (XcfInfo *info,
		GImage  *gimage)
{
  Layer *layer;
  Layer *floating_layer;
  Channel *channel;
  guint32 saved_pos;
  guint32 offset;
  guint nlayers;
  guint nchannels;
  GSList *list;
  int have_selection;
  int t1, t2, t3, t4;
  char version_tag[14];

  floating_layer = gimage_floating_sel (gimage);
  if (floating_layer)
    floating_sel_relax (floating_layer, FALSE);

  /* write out the tag information for the image */
  if (info->file_version > 0) 
    {
      sprintf (version_tag, "gimp xcf v%03d", info->file_version);
    } 
  else 
    {
      strcpy (version_tag, "gimp xcf file");
    }
  info->cp += xcf_write_int8 (info->fp, (guint8*) version_tag, 14);

  /* write out the width, height and image type information for the image */
  info->cp += xcf_write_int32 (info->fp, (guint32*) &gimage->width, 1);
  info->cp += xcf_write_int32 (info->fp, (guint32*) &gimage->height, 1);
  info->cp += xcf_write_int32 (info->fp, (guint32*) &gimage->base_type, 1);

  /* determine the number of layers and channels in the image */
  nlayers = (guint) g_slist_length (gimage->layers);
  nchannels = (guint) g_slist_length (gimage->channels);

  /* check and see if we have to save out the selection */
  have_selection = gimage_mask_bounds (gimage, &t1, &t2, &t3, &t4);
  if (have_selection)
    nchannels += 1;

  /* write the property information for the image.
   */
  xcf_save_image_props (info, gimage);

  /* save the current file position as it is the start of where
   *  we place the layer offset information.
   */
  saved_pos = info->cp;

  /* seek to after the offset lists */
  xcf_seek_pos (info, info->cp + (nlayers + nchannels + 2) * 4);

  list = gimage->layers;
  while (list)
    {
      layer = list->data;
      list = list->next;

      /* save the start offset of where we are writing
       *  out the next layer.
       */
      offset = info->cp;

      /* write out the layer. */
      xcf_save_layer (info, gimage, layer);

      /* seek back to where we are to write out the next
       *  layer offset and write it out.
       */
      xcf_seek_pos (info, saved_pos);
      info->cp += xcf_write_int32 (info->fp, &offset, 1);

      /* increment the location we are to write out the
       *  next offset.
       */
      saved_pos = info->cp;

      /* seek to the end of the file which is where
       *  we will write out the next layer.
       */
      xcf_seek_end (info);
    }

  /* write out a '0' offset position to indicate the end
   *  of the layer offsets.
   */
  offset = 0;
  xcf_seek_pos (info, saved_pos);
  info->cp += xcf_write_int32 (info->fp, &offset, 1);
  saved_pos = info->cp;
  xcf_seek_end (info);


  list = gimage->channels;
  while (list || have_selection)
    {
      if (list)
	{
	  channel = list->data;
	  list = list->next;
	}
      else
	{
	  channel = gimage->selection_mask;
	  have_selection = FALSE;
	}

      /* save the start offset of where we are writing
       *  out the next channel.
       */
      offset = info->cp;

      /* write out the layer. */
      xcf_save_channel (info, gimage, channel);

      /* seek back to where we are to write out the next
       *  channel offset and write it out.
       */
      xcf_seek_pos (info, saved_pos);
      info->cp += xcf_write_int32 (info->fp, &offset, 1);

      /* increment the location we are to write out the
       *  next offset.
       */
      saved_pos = info->cp;

      /* seek to the end of the file which is where
       *  we will write out the next channel.
       */
      xcf_seek_end (info);
    }

  /* write out a '0' offset position to indicate the end
   *  of the channel offsets.
   */
  offset = 0;
  xcf_seek_pos (info, saved_pos);
  info->cp += xcf_write_int32 (info->fp, &offset, 1);
  saved_pos = info->cp;

  if (floating_layer)
    floating_sel_rigor (floating_layer, FALSE);

  return !ferror(info->fp);
}

static void
xcf_save_image_props (XcfInfo *info,
		      GImage  *gimage)
{
  /* check and see if we should save the colormap property */
  if (gimage->cmap)
    xcf_save_prop (info, PROP_COLORMAP, gimage->num_cols, gimage->cmap);

  if (info->compression != COMPRESS_NONE)
    xcf_save_prop (info, PROP_COMPRESSION, info->compression);

  if (gimage->guides)
    xcf_save_prop (info, PROP_GUIDES, gimage->guides);

  xcf_save_prop (info, PROP_RESOLUTION,
		 gimage->xresolution, gimage->yresolution);

  xcf_save_prop (info, PROP_TATTOO, gimage->tattoo_state);

  if (parasite_list_length(gimage->parasites) > 0)
    xcf_save_prop (info, PROP_PARASITES, gimage->parasites);

  if (gimage->unit < gimp_unit_get_number_of_built_in_units ())
    xcf_save_prop (info, PROP_UNIT, gimage->unit);

  xcf_save_prop (info, PROP_PATHS, gimage->paths);

  if (gimage->unit >= gimp_unit_get_number_of_built_in_units ())
    xcf_save_prop (info, PROP_USER_UNIT, gimage->unit);

  xcf_save_prop (info, PROP_END);
}

static void
xcf_save_layer_props (XcfInfo *info,
		      GImage  *gimage,
		      Layer   *layer)
{
  if (layer == gimage->active_layer)
    xcf_save_prop (info, PROP_ACTIVE_LAYER);

  if (layer == gimage_floating_sel (gimage))
    {
      info->floating_sel_drawable = layer->fs.drawable;
      xcf_save_prop (info, PROP_FLOATING_SELECTION);
    }

  xcf_save_prop (info, PROP_OPACITY, layer->opacity);
  xcf_save_prop (info, PROP_VISIBLE, GIMP_DRAWABLE(layer)->visible);
  xcf_save_prop (info, PROP_LINKED, layer->linked);
  xcf_save_prop (info, PROP_PRESERVE_TRANSPARENCY, layer->preserve_trans);
  xcf_save_prop (info, PROP_APPLY_MASK, layer->apply_mask);
  xcf_save_prop (info, PROP_EDIT_MASK, layer->edit_mask);
  xcf_save_prop (info, PROP_SHOW_MASK, layer->show_mask);
  xcf_save_prop (info, PROP_OFFSETS, GIMP_DRAWABLE(layer)->offset_x, GIMP_DRAWABLE(layer)->offset_y);
  xcf_save_prop (info, PROP_MODE, layer->mode);
  xcf_save_prop (info, PROP_TATTOO, GIMP_DRAWABLE(layer)->tattoo);
  if (parasite_list_length(GIMP_DRAWABLE(layer)->parasites) > 0)
    xcf_save_prop (info, PROP_PARASITES, GIMP_DRAWABLE(layer)->parasites);

  xcf_save_prop (info, PROP_END);
}

static void
xcf_save_channel_props (XcfInfo *info,
			GImage  *gimage,
			Channel *channel)
{
  if (channel == gimage->active_channel)
    xcf_save_prop (info, PROP_ACTIVE_CHANNEL);

  if (channel == gimage->selection_mask)
    xcf_save_prop (info, PROP_SELECTION);

  xcf_save_prop (info, PROP_OPACITY, channel->opacity);
  xcf_save_prop (info, PROP_VISIBLE, GIMP_DRAWABLE(channel)->visible);
  xcf_save_prop (info, PROP_SHOW_MASKED, channel->show_masked);
  xcf_save_prop (info, PROP_COLOR, channel->col);
  xcf_save_prop (info, PROP_TATTOO, GIMP_DRAWABLE(channel)->tattoo);
  if (parasite_list_length(GIMP_DRAWABLE(channel)->parasites) > 0)
    xcf_save_prop (info, PROP_PARASITES, GIMP_DRAWABLE(channel)->parasites);

  xcf_save_prop (info, PROP_END);
}

static void write_a_parasite(char *key, Parasite *p, XcfInfo *info)
{
  if ((p->flags & PARASITE_PERSISTENT))
  {
    info->cp += xcf_write_string(info->fp, &p->name, 1);
    info->cp += xcf_write_int32(info->fp, &p->flags, 1);
    info->cp += xcf_write_int32(info->fp, &p->size, 1);
    info->cp += xcf_write_int8(info->fp, p->data, p->size);
  }
}

static Parasite *read_a_parasite(XcfInfo *info)
{
  Parasite *p;
  p = g_new(Parasite, 1);
  info->cp += xcf_read_string(info->fp, &p->name, 1);
  info->cp += xcf_read_int32(info->fp, &p->flags, 1);
  info->cp += xcf_read_int32(info->fp, &p->size, 1);
  p->data = g_new (char, p->size);
  info->cp += xcf_read_int8(info->fp, p->data, p->size);
  return p;
}

static void write_bz_point(gpointer pptr, gpointer iptr)
{
  PATHPOINTP bpt = (PATHPOINTP)pptr;
  XcfInfo *info = (XcfInfo *)iptr;
  gfloat   xfloat = (gfloat)bpt->x;
  gfloat   yfloat = (gfloat)bpt->y;

  /* (all gint)
   * type
   * x
   * y
   */

  info->cp += xcf_write_int32(info->fp, &bpt->type,1);
  info->cp += xcf_write_float(info->fp, &xfloat,1);
  info->cp += xcf_write_float(info->fp, &yfloat,1);
}

static PATHPOINTP v1read_bz_point(XcfInfo *info)
{
  PATHPOINTP ptr;
  guint32 type;
  gint32 x;
  gint32 y;

  info->cp += xcf_read_int32(info->fp, &type,1);
  info->cp += xcf_read_int32(info->fp, (guint32*)&x,1);
  info->cp += xcf_read_int32(info->fp, (guint32*)&y,1);

  ptr = pathpoint_new(type,(gdouble)x,(gdouble)y);

  return (ptr);
}

static PATHPOINTP read_bz_point(XcfInfo *info)
{
  PATHPOINTP ptr;
  guint32 type;
  gfloat x;
  gfloat y;
 
  info->cp += xcf_read_int32(info->fp, &type,1);
  info->cp += xcf_read_float(info->fp, &x,1);
  info->cp += xcf_read_float(info->fp, &y,1);
 
  ptr = pathpoint_new(type,(gdouble)x,(gdouble)y);

  return (ptr);
}

static void write_one_path(gpointer pptr, gpointer iptr)
{
  PATHP bzp = (PATHP)pptr;
  XcfInfo *info = (XcfInfo *)iptr;
  guint8 state = (gchar)bzp->state;
  guint32 num_points;
  guint32 closed;
  guint32 version;

  /*
   * name (string)
   * locked (gint)
   * state (gchar)
   * closed (gint)
   * number points (gint)
   * number paths (gint unused)
   * then each point.
   */
 
  info->cp += xcf_write_string(info->fp, &bzp->name->str, 1);
  info->cp += xcf_write_int32(info->fp, &bzp->locked,1);
  info->cp += xcf_write_int8(info->fp, &state,1);
  closed = bzp->closed;
  info->cp += xcf_write_int32(info->fp, &closed,1);
  num_points = g_slist_length(bzp->path_details);
  info->cp += xcf_write_int32(info->fp, &num_points,1);
  version = 3;
  info->cp += xcf_write_int32(info->fp, &version,1);
  info->cp += xcf_write_int32(info->fp, &bzp->pathtype,1);
  info->cp += xcf_write_int32(info->fp, &bzp->tattoo,1); 
  g_slist_foreach(bzp->path_details,write_bz_point,info);
}

static PATHP read_one_path(GImage *gimage,XcfInfo *info)
{
  PATHP bzp;
  gchar *name;
  guint32 locked;
  guint8  state;
  guint32 closed;
  guint32 num_points;
  guint32 version; /* changed from num_paths */
  Tattoo  tattoo = 0;
  GSList *pts_list = NULL;
  PathType ptype;

  info->cp += xcf_read_string(info->fp, &name, 1);
  info->cp += xcf_read_int32(info->fp, &locked,1);
  info->cp += xcf_read_int8(info->fp, &state,1);
  info->cp += xcf_read_int32(info->fp, &closed,1);
  info->cp += xcf_read_int32(info->fp, &num_points,1);
  info->cp += xcf_read_int32(info->fp, &version,1);

  if(version == 1)
    {
      ptype = BEZIER;
      while(num_points-- > 0)
      {
        PATHPOINTP bpt;
        /* Read in a path */
        bpt = v1read_bz_point(info);
        pts_list = g_slist_append(pts_list,bpt);
      }
    }
  else if(version == 2)
    {
      /* Had extra type field and points are stored as doubles */
      info->cp += xcf_read_int32(info->fp, (guint32 *)&ptype,1);
      while(num_points-- > 0)
      {
        PATHPOINTP bpt;
        /* Read in a path */
        bpt = read_bz_point(info);
        pts_list = g_slist_append(pts_list,bpt);
      }
    }
  else if(version == 3)
    {
      /* Has extra tatto field */
      info->cp += xcf_read_int32(info->fp, (guint32 *)&ptype,1);
      info->cp += xcf_read_int32(info->fp, (guint32 *)&tattoo,1);
      while(num_points-- > 0)
      {
        PATHPOINTP bpt;
        /* Read in a path */
        bpt = read_bz_point(info);
        pts_list = g_slist_append(pts_list,bpt);
      }
    }
  else
    {
      g_warning("Unknown path type..Possibly corrupt XCF file");
    }

  bzp = path_new(gimage,ptype,pts_list,closed,(gint)state,locked,tattoo,name);

  return(bzp);
}

static void write_bzpaths(PathsList *paths, XcfInfo *info)
{
  guint32 num_paths;
  /* Write out the following:-
   *
   * last_selected_row (gint)
   * number_of_paths (gint)
   *
   * then each path:-
   */
  
  info->cp += xcf_write_int32(info->fp,(guint32*)&paths->last_selected_row,1);
  num_paths = g_slist_length(paths->bz_paths);
  info->cp += xcf_write_int32(info->fp,&num_paths,1);
  g_slist_foreach(paths->bz_paths,write_one_path,info);  
}

static PathsList * read_bzpaths(GImage *gimage, XcfInfo *info)
{
  guint32 num_paths;
  guint32 last_selected_row;
  PathsList *paths;
  GSList *bzp_list = NULL;

  info->cp += xcf_read_int32(info->fp,&last_selected_row,1);
  info->cp += xcf_read_int32(info->fp,&num_paths,1);

  while(num_paths-- > 0)
    {
      PATHP bzp;
      /* Read in a path */
      bzp = read_one_path(gimage,info);
      bzp_list = g_slist_append(bzp_list,bzp);
    }

  paths = pathsList_new(gimage,last_selected_row,bzp_list);

  return(paths);
}

static void
xcf_save_prop (XcfInfo  *info,
	       PropType  prop_type,
	       ...)
{
  guint32 size;
  va_list args;

  va_start (args, prop_type);

  switch (prop_type)
    {
    case PROP_END:
      size = 0;

      info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
      info->cp += xcf_write_int32 (info->fp, &size, 1);
      break;
    case PROP_COLORMAP:
      {
	guint32 ncolors;
	guchar *colors;

	ncolors = va_arg (args, guint32);
	colors = va_arg (args, guchar*);
	size = 4 + ncolors;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);
	info->cp += xcf_write_int32 (info->fp, &ncolors, 1);
	info->cp += xcf_write_int8 (info->fp, colors, ncolors * 3);
      }
      break;
    case PROP_ACTIVE_LAYER:
    case PROP_ACTIVE_CHANNEL:
    case PROP_SELECTION:
      size = 0;

      info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
      info->cp += xcf_write_int32 (info->fp, &size, 1);
      break;
    case PROP_FLOATING_SELECTION:
      {
	guint32 dummy;

	dummy = 0;
	size = 4;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);
	info->floating_sel_offset = info->cp;
	info->cp += xcf_write_int32 (info->fp, &dummy, 1);
      }
      break;
    case PROP_OPACITY:
      {
	gint32 opacity;

	opacity = va_arg (args, gint32);

	size = 4;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);
	info->cp += xcf_write_int32 (info->fp, (guint32*) &opacity, 1);
      }
      break;
    case PROP_MODE:
      {
	gint32 mode;

	mode = va_arg (args, gint32);
	size = 4;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);
	info->cp += xcf_write_int32 (info->fp, (guint32*) &mode, 1);
      }
      break;
    case PROP_VISIBLE:
      {
	guint32 visible;

	visible = va_arg (args, guint32);
	size = 4;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);
	info->cp += xcf_write_int32 (info->fp, &visible, 1);
      }
      break;
    case PROP_LINKED:
      {
	guint32 linked;

	linked = va_arg (args, guint32);
	size = 4;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);
	info->cp += xcf_write_int32 (info->fp, &linked, 1);
      }
      break;
    case PROP_PRESERVE_TRANSPARENCY:
      {
	guint32 preserve_trans;

	preserve_trans = va_arg (args, guint32);
	size = 4;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);
	info->cp += xcf_write_int32 (info->fp, &preserve_trans, 1);
      }
      break;
    case PROP_APPLY_MASK:
      {
	guint32 apply_mask;

	apply_mask = va_arg (args, guint32);
	size = 4;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);
	info->cp += xcf_write_int32 (info->fp, &apply_mask, 1);
      }
      break;
    case PROP_EDIT_MASK:
      {
	guint32 edit_mask;

	edit_mask = va_arg (args, guint32);
	size = 4;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);
	info->cp += xcf_write_int32 (info->fp, &edit_mask, 1);
      }
      break;
    case PROP_SHOW_MASK:
      {
	guint32 show_mask;

	show_mask = va_arg (args, guint32);
	size = 4;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);
	info->cp += xcf_write_int32 (info->fp, &show_mask, 1);
      }
      break;
    case PROP_SHOW_MASKED:
      {
	guint32 show_masked;

	show_masked = va_arg (args, guint32);
	size = 4;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);
	info->cp += xcf_write_int32 (info->fp, &show_masked, 1);
      }
      break;
    case PROP_OFFSETS:
      {
	gint32 offsets[2];

	offsets[0] = va_arg (args, gint32);
	offsets[1] = va_arg (args, gint32);
	size = 8;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);
	info->cp += xcf_write_int32 (info->fp, (guint32*) offsets, 2);
      }
      break;
    case PROP_COLOR:
      {
	guchar *color;

	color = va_arg (args, guchar*);
	size = 3;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);
	info->cp += xcf_write_int8 (info->fp, color, 3);
      }
      break;
    case PROP_COMPRESSION:
      {
	guint8 compression;

	compression = (guint8) va_arg (args, guint32);
	size = 1;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);
	info->cp += xcf_write_int8 (info->fp, &compression, 1);
      }
      break;
    case PROP_GUIDES:
      {
	GList *guides;
	Guide *guide;
	gint32 position;
	gint8 orientation;
	int nguides;

	guides = va_arg (args, GList*);
	nguides = g_list_length (guides);

	size = nguides * (4 + 1);

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);

	while (guides)
	  {
	    guide = guides->data;
	    guides = guides->next;

	    position = guide->position;
	    orientation = guide->orientation;

	    info->cp += xcf_write_int32 (info->fp, (guint32*) &position, 1);
	    info->cp += xcf_write_int8 (info->fp, (guint8*) &orientation, 1);
	  }
      }
      break;
    case PROP_RESOLUTION:
      {
	float xresolution, yresolution;

	/* we pass in floats, but they are promoted to double by the compiler */
	xresolution =  va_arg (args, double);
	yresolution =  va_arg (args, double);
	
	size = 4*2;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);

	info->cp += xcf_write_float (info->fp, &xresolution, 1);
	info->cp += xcf_write_float (info->fp, &yresolution, 1);
      }
      break;
    case PROP_TATTOO:
      {
	guint32 tattoo;

	tattoo =  va_arg (args, guint32);
	size = 4;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);
	info->cp += xcf_write_int32 (info->fp, &tattoo, 1);
      }
      break;
    case PROP_PARASITES:
      {
	ParasiteList *list;
	guint32 base, length;
	long pos;
	list =  va_arg (args, ParasiteList*);
	if (parasite_list_persistent_length(list) > 0)
	{
 	  info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	  /* because we don't know how much room the parasite list will take
	     we save the file position and write the length later */
	  pos = ftell(info->fp);
 	  info->cp += xcf_write_int32 (info->fp, &length, 1);
	  base = info->cp;
	  parasite_list_foreach(list, (GHFunc) write_a_parasite, info);
	  length = info->cp - base;
	  /* go back to the saved position and write the length */
	  fseek(info->fp, pos, SEEK_SET);
	  xcf_write_int32 (info->fp, &length, 1);
	  fseek(info->fp, 0, SEEK_END);
	}
      }
      break;
    case PROP_UNIT:
      {
	guint32 unit;

	unit = va_arg (args, guint32);

	size = 4;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);
	info->cp += xcf_write_int32 (info->fp, &unit, 1);
      }
      break;
    case PROP_PATHS:
      {
	PathsList *paths_list;
	guint32 base, length;
	long pos;
	paths_list =  va_arg (args, PathsList *);
	if (paths_list)
	{
 	  info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	  /* because we don't know how much room the paths list will take
	     we save the file position and write the length later 
	     ALT. OK I copied the code from above...
	  */
	  pos = ftell(info->fp);
 	  info->cp += xcf_write_int32 (info->fp, &length, 1);
	  base = info->cp;
	  write_bzpaths(paths_list,info);
	  length = info->cp - base;
	  /* go back to the saved position and write the length */
	  fseek(info->fp, pos, SEEK_SET);
	  xcf_write_int32 (info->fp, &length, 1);
	  fseek(info->fp, 0, SEEK_END);
	}
      }
      break;
    case PROP_USER_UNIT:
      {
	GUnit   unit;
	gchar  *unit_strings[5];
	float   factor;
	guint32 digits;

	unit = va_arg (args, guint32);

	/* write the entire unit definition */
	unit_strings[0] = gimp_unit_get_identifier (unit);
	factor          = gimp_unit_get_factor (unit);
	digits          = gimp_unit_get_digits (unit);
	unit_strings[1] = gimp_unit_get_symbol (unit);
	unit_strings[2] = gimp_unit_get_abbreviation (unit);
	unit_strings[3] = gimp_unit_get_singular (unit);
	unit_strings[4] = gimp_unit_get_plural (unit);

	size =
	  2 * 4 +
	  strlen (unit_strings[0]) ? strlen (unit_strings[0]) + 5 : 4 +
	  strlen (unit_strings[1]) ? strlen (unit_strings[1]) + 5 : 4 +
	  strlen (unit_strings[2]) ? strlen (unit_strings[2]) + 5 : 4 +
	  strlen (unit_strings[3]) ? strlen (unit_strings[3]) + 5 : 4 +
	  strlen (unit_strings[4]) ? strlen (unit_strings[4]) + 5 : 4;

	info->cp += xcf_write_int32 (info->fp, (guint32*) &prop_type, 1);
	info->cp += xcf_write_int32 (info->fp, &size, 1);
	info->cp += xcf_write_float (info->fp, &factor, 1);
	info->cp += xcf_write_int32 (info->fp, &digits, 1);
	info->cp += xcf_write_string (info->fp, unit_strings, 5);
      }
      break;
    }

  va_end (args);
}

static void
xcf_save_layer (XcfInfo *info,
		GImage  *gimage,
		Layer   *layer)
{
  guint32 saved_pos;
  guint32 offset;

  /* check and see if this is the drawable that the floating
   *  selection is attached to.
   */
  if (GIMP_DRAWABLE(layer) == info->floating_sel_drawable)
    {
      saved_pos = info->cp;
      xcf_seek_pos (info, info->floating_sel_offset);
      info->cp += xcf_write_int32 (info->fp, &saved_pos, 1);
      xcf_seek_pos (info, saved_pos);
    }

  /* write out the width, height and image type information for the layer */
  info->cp += xcf_write_int32 (info->fp, (guint32*) &GIMP_DRAWABLE(layer)->width, 1);
  info->cp += xcf_write_int32 (info->fp, (guint32*) &GIMP_DRAWABLE(layer)->height, 1);
  info->cp += xcf_write_int32 (info->fp, (guint32*) &GIMP_DRAWABLE(layer)->type, 1);

  /* write out the layers name */
  info->cp += xcf_write_string (info->fp, &GIMP_DRAWABLE(layer)->name, 1);

  /* write out the layer properties */
  xcf_save_layer_props (info, gimage, layer);

  /* save the current position which is where the hierarchy offset
   *  will be stored.
   */
  saved_pos = info->cp;

  /* write out the layer tile hierarchy */
  xcf_seek_pos (info, info->cp + 8);
  offset = info->cp;

  xcf_save_hierarchy (info, GIMP_DRAWABLE(layer)->tiles);

  xcf_seek_pos (info, saved_pos);
  info->cp += xcf_write_int32 (info->fp, &offset, 1);
  saved_pos = info->cp;

  /* write out the layer mask */
  if (layer->mask)
    {
      xcf_seek_end (info);
      offset = info->cp;

      xcf_save_channel (info, gimage, GIMP_CHANNEL(layer->mask));
    }
  else
    offset = 0;

  xcf_seek_pos (info, saved_pos);
  info->cp += xcf_write_int32 (info->fp, &offset, 1);
}

static void
xcf_save_channel (XcfInfo *info,
		  GImage  *gimage,
		  Channel *channel)
{
  guint32 saved_pos;
  guint32 offset;

  /* check and see if this is the drawable that the floating
   *  selection is attached to.
   */
  if (GIMP_DRAWABLE(channel) == info->floating_sel_drawable)
    {
      saved_pos = info->cp;
      xcf_seek_pos (info, info->floating_sel_offset);
      info->cp += xcf_write_int32 (info->fp, (guint32*) &info->cp, 1);
      xcf_seek_pos (info, saved_pos);
    }

  /* write out the width and height information for the channel */
  info->cp += xcf_write_int32 (info->fp, (guint32*) &GIMP_DRAWABLE(channel)->width, 1);
  info->cp += xcf_write_int32 (info->fp, (guint32*) &GIMP_DRAWABLE(channel)->height, 1);

  /* write out the channels name */
  info->cp += xcf_write_string (info->fp, &GIMP_DRAWABLE(channel)->name, 1);

  /* write out the channel properties */
  xcf_save_channel_props (info, gimage, channel);

  /* save the current position which is where the hierarchy offset
   *  will be stored.
   */
  saved_pos = info->cp;

  /* write out the channel tile hierarchy */
  xcf_seek_pos (info, info->cp + 4);
  offset = info->cp;

  xcf_save_hierarchy (info, GIMP_DRAWABLE(channel)->tiles);

  xcf_seek_pos (info, saved_pos);
  info->cp += xcf_write_int32 (info->fp, &offset, 1);
  saved_pos = info->cp;
}

static int
xcf_calc_levels (int size,
		 int tile_size)
{
  int levels;

  levels = 1;
  while (size > tile_size)
    {
      size /= 2;
      levels += 1;
    }

  return levels;
}


static void
xcf_save_hierarchy (XcfInfo     *info,
		    TileManager *tiles)
{
  guint32 saved_pos;
  guint32 offset;
  int i;
  int nlevels, tmp1, tmp2;
  int h, w;

  info->cp += xcf_write_int32 (info->fp, (guint32*) &tiles->width, 1);
  info->cp += xcf_write_int32 (info->fp, (guint32*) &tiles->height, 1);
  info->cp += xcf_write_int32 (info->fp, (guint32*) &tiles->bpp, 1);

  saved_pos = info->cp;
  
  tmp1 = xcf_calc_levels (tiles->width, TILE_WIDTH);
  tmp2 = xcf_calc_levels (tiles->height, TILE_HEIGHT);
  nlevels = MAX(tmp1, tmp2);

  xcf_seek_pos (info, info->cp + (1 + nlevels) * 4);

  for (i = 0; i < nlevels; i++) 
    {
      offset = info->cp;

      if (i == 0) 
	{
	  /* write out the level. */
	  xcf_save_level (info, tiles);
	  w = tiles->width;
	  h = tiles->height;
	} 
      else 
	{
	  /* fake an empty level */
	  tmp1 = 0;
	  w /= 2;
	  h /= 2;
	  info->cp += xcf_write_int32 (info->fp, (guint32*) &w, 1);
	  info->cp += xcf_write_int32 (info->fp, (guint32*) &h, 1);
	  info->cp += xcf_write_int32 (info->fp, (guint32*) &tmp1, 1);
	}

      /* seek back to where we are to write out the next
       *  level offset and write it out.
       */
      xcf_seek_pos (info, saved_pos);
      info->cp += xcf_write_int32 (info->fp, &offset, 1);
      
      /* increment the location we are to write out the
       *  next offset.
       */
      saved_pos = info->cp;
  
      /* seek to the end of the file which is where
       *  we will write out the next level.
       */
      xcf_seek_end (info);
    }

  /* write out a '0' offset position to indicate the end
   *  of the level offsets.
   */
  offset = 0;
  xcf_seek_pos (info, saved_pos);
  info->cp += xcf_write_int32 (info->fp, &offset, 1);
}

static void
xcf_save_level (XcfInfo     *info,
		TileManager *level)
{
  guint32 saved_pos;
  guint32 offset;
  guint ntiles;
  int i;
  guchar *rlebuf;

  info->cp += xcf_write_int32 (info->fp, (guint32*) &level->width, 1);
  info->cp += xcf_write_int32 (info->fp, (guint32*) &level->height, 1);

  saved_pos = info->cp;

  /* allocate a temporary buffer to store the rle data before it is
     written to disk */
  rlebuf = g_malloc(TILE_WIDTH*TILE_HEIGHT*level->bpp * 1.5);

  if (level->tiles)
    {
      ntiles = level->ntile_rows * level->ntile_cols;
      xcf_seek_pos (info, info->cp + (ntiles + 1) * 4);

      for (i = 0; i < ntiles; i++)
	{
	  /* save the start offset of where we are writing
	   *  out the next tile.
	   */
	  offset = info->cp;

	  /* write out the tile. */
	  switch (info->compression)
	    {
	    case COMPRESS_NONE:
	      xcf_save_tile (info, level->tiles[i]);
	      break;
	    case COMPRESS_RLE:
	      xcf_save_tile_rle (info, level->tiles[i], rlebuf);
	      break;
	    case COMPRESS_ZLIB:
	      g_error (_("xcf: zlib compression unimplemented"));
	      break;
	    case COMPRESS_FRACTAL:
	      g_error (_("xcf: fractal compression unimplemented"));
	      break;
	    }

	  /* seek back to where we are to write out the next
	   *  tile offset and write it out.
	   */
	  xcf_seek_pos (info, saved_pos);
	  info->cp += xcf_write_int32 (info->fp, &offset, 1);

	  /* increment the location we are to write out the
	   *  next offset.
	   */
	  saved_pos = info->cp;

	  /* seek to the end of the file which is where
	   *  we will write out the next tile.
	   */
	  xcf_seek_end (info);
	}
    }

  g_free(rlebuf);

  /* write out a '0' offset position to indicate the end
   *  of the level offsets.
   */
  offset = 0;
  xcf_seek_pos (info, saved_pos);
  info->cp += xcf_write_int32 (info->fp, &offset, 1);
}

static void
xcf_save_tile (XcfInfo *info,
	       Tile    *tile)
{
  tile_lock (tile);
  info->cp += xcf_write_int8 (info->fp, tile_data_pointer (tile, 0, 0), 
			      tile_size (tile));
  tile_release (tile, FALSE);
}

static void
xcf_save_tile_rle (XcfInfo *info,
		   Tile    *tile,
		   guchar  *rlebuf)
{
  guchar *data, *t;
  unsigned int last;
  int state;
  int length;
  int count;
  int size;
  int bpp;
  int i, j;
  int len = 0;

  tile_lock (tile);

  bpp = tile_bpp (tile);

  for (i = 0; i < bpp; i++)
    {
      data = (guchar*)tile_data_pointer (tile, 0, 0) + i;

      state = 0;
      length = 0;
      count = 0;
      size = tile_ewidth(tile) * tile_eheight(tile);
      last = -1;

      while (size > 0)
	{
	  switch (state)
	    {
	    case 0:
	      /* in state 0 we try to find a long sequence of
	       *  matching values.
	       */
	      if ((length == 32768) ||
		  ((size - length) <= 0) ||
		  ((length > 1) && (last != *data)))
		{
		  count += length;
		  if (length >= 128)
		    {
		      rlebuf[len++] = 127;
                      rlebuf[len++] = (length >> 8);
                      rlebuf[len++] = length & 0x00FF;
		      rlebuf[len++] = last;
		    }
		  else
		    {
		      rlebuf[len++] = length - 1;
		      rlebuf[len++] = last;
		    }
		  size -= length;
		  length = 0;
		}
	      else if ((length == 1) && (last != *data))
		state = 1;
	      break;
	    case 1:
	      /* in state 1 we try and find a long sequence of
	       *  non-matching values.
	       */
	      if ((length == 32768) ||
		  ((size - length) == 0) ||
		  ((length > 0) && (last == *data) && 
		   ((size - length) == 1 || last == data[bpp])))
		{
		  count += length;
		  state = 0;

		  if (length >= 128)
		    {
		      rlebuf[len++] = 255 - 127;
                      rlebuf[len++] = (length >> 8);
                      rlebuf[len++] = length & 0x00FF;
		    }
		  else
		    {
		      rlebuf[len++] = 255 - (length - 1);
		    }

		  t = data - length * bpp;
		  for (j = 0; j < length; j++)
		    {
		      rlebuf[len++] = *t;
		      t += bpp;
		    }

		  size -= length;
		  length = 0;
		}
	      break;
	    }

	  if (size > 0) {
	    length += 1;
	    last = *data;
	    data += bpp;
	  }
	}

      if (count != (tile_ewidth (tile) * tile_eheight (tile)))
	g_message (_("xcf: uh oh! xcf rle tile saving error: %d"), count);
    }
  info->cp += xcf_write_int8(info->fp, rlebuf, len);
  tile_release (tile, FALSE);
}


static GImage*
xcf_load_image (XcfInfo *info)
{
  GImage *gimage;
  Layer *layer;
  Channel *channel;
  guint32 saved_pos;
  guint32 offset;
  int width;
  int height;
  int image_type;

  /* read in the image width, height and type */
  info->cp += xcf_read_int32 (info->fp, (guint32*) &width, 1);
  info->cp += xcf_read_int32 (info->fp, (guint32*) &height, 1);
  info->cp += xcf_read_int32 (info->fp, (guint32*) &image_type, 1);

  /* create a new gimage */
  gimage = gimage_new (width, height, image_type);
  if (!gimage)
    return NULL;

  /* read the image properties */
  if (!xcf_load_image_props (info, gimage))
    goto error;

  while (1)
    {
      /* read in the offset of the next layer */
      info->cp += xcf_read_int32 (info->fp, &offset, 1);

      /* if the offset is 0 then we are at the end
       *  of the layer list.
       */
      if (offset == 0)
	break;

      /* save the current position as it is where the
       *  next layer offset is stored.
       */
      saved_pos = info->cp;

      /* seek to the layer offset */
      xcf_seek_pos (info, offset);

      /* read in the layer */
      layer = xcf_load_layer (info, gimage);
      if (!layer)
	goto error;

      /* add the layer to the image if its not the floating selection */
      if (layer != info->floating_sel)
	gimage_add_layer (gimage, layer, g_slist_length (gimage->layers));

      /* restore the saved position so we'll be ready to
       *  read the next offset.
       */
      xcf_seek_pos (info, saved_pos);
    }

  while (1)
    {
      /* read in the offset of the next channel */
      info->cp += xcf_read_int32 (info->fp, &offset, 1);

      /* if the offset is 0 then we are at the end
       *  of the channel list.
       */
      if (offset == 0)
	break;

      /* save the current position as it is where the
       *  next channel offset is stored.
       */
      saved_pos = info->cp;

      /* seek to the channel offset */
      xcf_seek_pos (info, offset);

      /* read in the layer */
      channel = xcf_load_channel (info, gimage);
      if (!channel)
	goto error;

      /* add the channel to the image if its not the selection */
      if (channel != gimage->selection_mask)
	gimage_add_channel (gimage, channel, -1);

      /* restore the saved position so we'll be ready to
       *  read the next offset.
       */
      xcf_seek_pos (info, saved_pos);
    }

  if (info->active_layer)
    gimage_set_active_layer (gimage, info->active_layer);

  if (info->active_channel)
    gimage_set_active_channel (gimage, info->active_channel);

  gimage_set_filename (gimage, info->filename);

  return gimage;

error:
  gimage_delete (gimage);
  return NULL;
}

static gint
xcf_load_image_props (XcfInfo *info,
		      GImage  *gimage)
{
  PropType prop_type;
  guint32 prop_size;

  while (1)
    {
      if (!xcf_load_prop (info, &prop_type, &prop_size))
	return FALSE;

      switch (prop_type)
	{
	case PROP_END:
	  return TRUE;
	case PROP_COLORMAP:
	  if (info->file_version == 0) 
	    {
	      int i;
	      g_message (_("XCF warning: version 0 of XCF file format\n"
			   "did not save indexed colormaps correctly.\n"
			   "Substituting grayscale map."));
	      info->cp += xcf_read_int32 (info->fp, (guint32*) &gimage->num_cols, 1);
	      gimage->cmap = g_new (guchar, gimage->num_cols*3);
	      xcf_seek_pos (info, info->cp + gimage->num_cols);
	      for (i = 0; i<gimage->num_cols; i++) 
		{
		  gimage->cmap[i*3+0] = i;
		  gimage->cmap[i*3+1] = i;
		  gimage->cmap[i*3+2] = i;
		}
	    }
	  else 
	    {
	      info->cp += xcf_read_int32 (info->fp, (guint32*) &gimage->num_cols, 1);
	      gimage->cmap = g_new (guchar, gimage->num_cols*3);
	      info->cp += xcf_read_int8 (info->fp, (guint8*) gimage->cmap, gimage->num_cols*3);
	    }
	  break;
	case PROP_COMPRESSION:
	  {
	    guint8 compression;

	    info->cp += xcf_read_int8 (info->fp, (guint8*) &compression, 1);

	    if ((compression != COMPRESS_NONE) &&
		(compression != COMPRESS_RLE) &&
		(compression != COMPRESS_ZLIB) &&
		(compression != COMPRESS_FRACTAL))
	      {
		g_message (_("unknown compression type: %d"), (int) compression);
		return FALSE;
	      }

	    info->compression = compression;
	  }
	  break;
	case PROP_GUIDES:
	  {
	    Guide *guide;
	    gint32 position;
	    gint8 orientation;
	    int i, nguides;

	    nguides = prop_size / (4 + 1);
	    for (i = 0; i < nguides; i++)
	      {
		info->cp += xcf_read_int32 (info->fp, (guint32*) &position, 1);
		info->cp += xcf_read_int8 (info->fp, (guint8*) &orientation, 1);

		guide = g_new (Guide, 1);
		guide->ref_count = 0;
		guide->position = position;
		guide->orientation = orientation;
		guide->guide_ID = next_guide_id++;

		gimage->guides = g_list_prepend (gimage->guides, guide);
	      }

	    gimage->guides = g_list_reverse (gimage->guides);
	  }
	  break;
	 case PROP_RESOLUTION:
	 {
	   float xres, yres;

	   info->cp += xcf_read_float (info->fp, &xres, 1);
	   info->cp += xcf_read_float (info->fp, &yres, 1);
	   if (xres < GIMP_MIN_RESOLUTION || xres > GIMP_MAX_RESOLUTION ||
	       yres < GIMP_MIN_RESOLUTION || yres > GIMP_MAX_RESOLUTION)
	     {
	       g_message(_("Warning, resolution out of range in XCF file"));
	       xres = yres = 72.0;
	     }
	   gimage->xresolution = xres;
	   gimage->yresolution = yres;
	 }
	 break;
	 case PROP_TATTOO:
	 {
	   info->cp += xcf_read_int32 (info->fp, &gimage->tattoo_state, 1);
	 }
	 break;
	 case PROP_PARASITES:
	 {
	   long base = info->cp;
	   Parasite *p;
	   while (info->cp - base < prop_size)
	   {
	     p = read_a_parasite(info);
	     gimp_image_attach_parasite(gimage, p);
	     parasite_free(p);
	   }
	   if (info->cp - base != prop_size)
	     g_message(_("Error detected while loading an image's parasites"));
	 }
	 break;
	 case PROP_UNIT:
	 {
	   guint32 unit;

	   info->cp += xcf_read_int32 (info->fp, &unit, 1);
	   
	   if ((unit >= gimp_unit_get_number_of_units()) )
	     {
	       g_message(_("Warning, unit out of range in XCF file, falling back to inches"));
	       unit = UNIT_INCH;
	     }

	   gimage->unit = unit;
	 }
	 break;
	case PROP_PATHS:
	  {
	    PathsList *paths = read_bzpaths(gimage,info);
	    /* add to gimage */
	    gimp_image_set_paths(gimage,paths);
	  }
	  break;
	case PROP_USER_UNIT:
	  {
	    gchar  *unit_strings[5];
	    float   factor;
	    guint32 digits;
	    GUnit   unit;
	    gint    num_units;
	    gint    i;

	    info->cp += xcf_read_float (info->fp, &factor, 1);
	    info->cp += xcf_read_int32 (info->fp, &digits, 1);
	    info->cp += xcf_read_string (info->fp, unit_strings, 5);

	    for (i = 0; i < 5; i++)
	      if (unit_strings[i] == NULL)
		unit_strings[i] = g_strdup ("");

	    num_units = gimp_unit_get_number_of_units ();
	    
	    for (unit = gimp_unit_get_number_of_built_in_units ();
		 unit < num_units; unit++)
	      {
		/* if the factor and the identifier match some unit
		 * in unitrc, use the unitrc unit
		 */
		if ((ABS (gimp_unit_get_factor (unit) - factor) < 1e-5) &&
		    (strcmp (unit_strings[0],
			     gimp_unit_get_identifier (unit)) == 0))
		  {
		    break;
		  }
	      }
	    
	    /* no match */
	    if (unit == num_units)
	      unit = gimp_unit_new (unit_strings[0],
				    factor,
				    digits,
				    unit_strings[1],
				    unit_strings[2],
				    unit_strings[3],
				    unit_strings[4]);

	    gimage->unit = unit;

	    for (i = 0; i < 5; i++)
	      g_free (unit_strings[i]);
	  }
	 break;
	default:
	  g_message (_("unexpected/unknown image property: %d (skipping)"), prop_type);

	  {
	    guint8 buf[16];
	    guint amount;

	    while (prop_size > 0)
	      {
		amount = MIN (16, prop_size);
		info->cp += xcf_read_int8 (info->fp, buf, amount);
		prop_size -= MIN (16, amount);
	      }
	  }
	  break;
	}
    }

  return FALSE;
}

static gint
xcf_load_layer_props (XcfInfo *info,
		      GImage  *gimage,
		      Layer   *layer)
{
  PropType prop_type;
  guint32 prop_size;

  while (1)
    {
      if (!xcf_load_prop (info, &prop_type, &prop_size))
	return FALSE;

      switch (prop_type)
	{
	case PROP_END:
	  return TRUE;
	case PROP_ACTIVE_LAYER:
	  info->active_layer = layer;
	  break;
	case PROP_FLOATING_SELECTION:
	  info->floating_sel = layer;
	  info->cp += xcf_read_int32 (info->fp, (guint32*) &info->floating_sel_offset, 1);
	  break;
	case PROP_OPACITY:
	  info->cp += xcf_read_int32 (info->fp, (guint32*) &layer->opacity, 1);
	  break;
	case PROP_VISIBLE:
	  info->cp += xcf_read_int32 (info->fp, (guint32*) &GIMP_DRAWABLE(layer)->visible, 1);
	  break;
	case PROP_LINKED:
	  info->cp += xcf_read_int32 (info->fp, (guint32*) &layer->linked, 1);
	  break;
	case PROP_PRESERVE_TRANSPARENCY:
	  info->cp += xcf_read_int32 (info->fp, (guint32*) &layer->preserve_trans, 1);
	  break;
	case PROP_APPLY_MASK:
	  info->cp += xcf_read_int32 (info->fp, (guint32*) &layer->apply_mask, 1);
	  break;
	case PROP_EDIT_MASK:
	  info->cp += xcf_read_int32 (info->fp, (guint32*) &layer->edit_mask, 1);
	  break;
	case PROP_SHOW_MASK:
	  info->cp += xcf_read_int32 (info->fp, (guint32*) &layer->show_mask, 1);
	  break;
	case PROP_OFFSETS:
	  info->cp += xcf_read_int32 (info->fp, (guint32*) &GIMP_DRAWABLE(layer)->offset_x, 1);
	  info->cp += xcf_read_int32 (info->fp, (guint32*) &GIMP_DRAWABLE(layer)->offset_y, 1);
	  break;
	case PROP_MODE:
	  info->cp += xcf_read_int32 (info->fp, (guint32*) &layer->mode, 1);
	  break;
	case PROP_TATTOO:
	  info->cp += xcf_read_int32 (info->fp,
				      (guint32*) &GIMP_DRAWABLE(layer)->tattoo,
				      1);
	  break;
	 case PROP_PARASITES:
	 {
	   long base = info->cp;
	   Parasite *p;
	   while (info->cp - base < prop_size)
	   {
	     p = read_a_parasite(info);
	     gimp_drawable_attach_parasite(GIMP_DRAWABLE(layer), p);
	     parasite_free(p);
	   }
	   if (info->cp - base != prop_size)
	     g_message("Error detected while loading a layer's parasites");
	 }
	 break;
	default:
	  g_message (_("unexpected/unknown layer property: %d (skipping)"), prop_type);

	  {
	    guint8 buf[16];
	    guint amount;

	    while (prop_size > 0)
	      {
		amount = MIN (16, prop_size);
		info->cp += xcf_read_int8 (info->fp, buf, amount);
		prop_size -= MIN (16, amount);
	      }
	  }
	  break;
	}
    }

  return FALSE;
}

static gint
xcf_load_channel_props (XcfInfo *info,
			GImage  *gimage,
			Channel *channel)
{
  PropType prop_type;
  guint32 prop_size;

  while (1)
    {
      if (!xcf_load_prop (info, &prop_type, &prop_size))
	return FALSE;

      switch (prop_type)
	{
	case PROP_END:
	  return TRUE;
	case PROP_ACTIVE_CHANNEL:
	  info->active_channel = channel;
	  break;
	case PROP_SELECTION:
	  channel_delete (gimage->selection_mask);
	  gimage->selection_mask = channel;
	  channel->boundary_known = FALSE;
	  channel->bounds_known   = FALSE;
	  break;
	case PROP_OPACITY:
	  info->cp += xcf_read_int32 (info->fp, (guint32*) &channel->opacity, 1);
	  break;
	case PROP_VISIBLE:
	  info->cp += xcf_read_int32 (info->fp, (guint32*) &GIMP_DRAWABLE(channel)->visible, 1);
	  break;
	case PROP_SHOW_MASKED:
	  info->cp += xcf_read_int32 (info->fp, (guint32*) &channel->show_masked, 1);
	  break;
	case PROP_COLOR:
	  info->cp += xcf_read_int8 (info->fp, (guint8*) channel->col, 3);
	  break;
	case PROP_TATTOO:
	  info->cp += xcf_read_int32 (info->fp, &GIMP_DRAWABLE(channel)->tattoo,
				      1);
	  break;
	 case PROP_PARASITES:
	 {
	   long base = info->cp;
	   Parasite *p;
	   while ((info->cp - base) < prop_size)
	   {
	     p = read_a_parasite(info);
	     gimp_drawable_attach_parasite(GIMP_DRAWABLE(channel), p);
	     parasite_free(p);
	   }
	   if (info->cp - base != prop_size)
	     g_message("Error detected while loading a channel's parasites");
	 }
	 break;
	default:
	  g_message (_("unexpected/unknown channel property: %d (skipping)"), prop_type);

	  {
	    guint8 buf[16];
	    guint amount;

	    while (prop_size > 0)
	      {
		amount = MIN (16, prop_size);
		info->cp += xcf_read_int8 (info->fp, buf, amount);
		prop_size -= MIN (16, amount);
	      }
	  }
	  break;
	}
    }

  return FALSE;
}

static gint
xcf_load_prop (XcfInfo  *info,
	       PropType *prop_type,
	       guint32  *prop_size)
{
  info->cp += xcf_read_int32 (info->fp, (guint32*) prop_type, 1);
  info->cp += xcf_read_int32 (info->fp, (guint32*) prop_size, 1);
  return TRUE;
}

static Layer*
xcf_load_layer (XcfInfo *info,
		GImage  *gimage)
{
  Layer *layer;
  LayerMask *layer_mask;
  guint32 hierarchy_offset;
  guint32 layer_mask_offset;
  gboolean apply_mask;
  gboolean edit_mask;
  gboolean show_mask;
  gint width;
  gint height;
  gint type;
  gint add_floating_sel;
  gchar *name;

  /* check and see if this is the drawable the floating selection
   *  is attached to. if it is then we'll do the attachment at
   *  the end of this function.
   */
  add_floating_sel = (info->cp == info->floating_sel_offset);

  /* read in the layer width, height, type and name */
  info->cp += xcf_read_int32 (info->fp, (guint32*) &width, 1);
  info->cp += xcf_read_int32 (info->fp, (guint32*) &height, 1);
  info->cp += xcf_read_int32 (info->fp, (guint32*) &type, 1);
  info->cp += xcf_read_string (info->fp, &name, 1);

  /* create a new layer */
  layer = layer_new (gimage, width, height, type, name, 255, NORMAL_MODE);
  if (!layer)
    {
      g_free (name);
      return NULL;
    }

  /* read in the layer properties */
  if (!xcf_load_layer_props (info, gimage, layer))
    goto error;

  /* read the hierarchy and layer mask offsets */
  info->cp += xcf_read_int32 (info->fp, &hierarchy_offset, 1);
  info->cp += xcf_read_int32 (info->fp, &layer_mask_offset, 1);

  /* read in the hierarchy */
  xcf_seek_pos (info, hierarchy_offset);
  if (!xcf_load_hierarchy (info, GIMP_DRAWABLE(layer)->tiles))
    goto error;

  /* read in the layer mask */
  if (layer_mask_offset != 0)
    {
      xcf_seek_pos (info, layer_mask_offset);

      layer_mask = xcf_load_layer_mask (info, gimage);
      if (!layer_mask)
	goto error;

      /* set the offsets of the layer_mask */
      GIMP_DRAWABLE(layer_mask)->offset_x = GIMP_DRAWABLE(layer)->offset_x;
      GIMP_DRAWABLE(layer_mask)->offset_y = GIMP_DRAWABLE(layer)->offset_y;

      apply_mask = layer->apply_mask;
      edit_mask  = layer->edit_mask;
      show_mask  = layer->show_mask;

      layer_add_mask (layer, layer_mask);

      layer->apply_mask = apply_mask;
      layer->edit_mask  = edit_mask;
      layer->show_mask  = show_mask;
    }

  /* attach the floating selection... */
  if (add_floating_sel)
    {
      Layer *floating_sel;

      floating_sel = info->floating_sel;
      floating_sel_attach (floating_sel, GIMP_DRAWABLE(layer));
    }

  return layer;

error:
  layer_delete (layer);
  return NULL;
}

static Channel*
xcf_load_channel (XcfInfo *info,
		  GImage  *gimage)
{
  Channel *channel;
  guint32 hierarchy_offset;
  int width;
  int height;
  int add_floating_sel;
  char *name;
  guchar color[3] = { 0, 0, 0 };

  /* check and see if this is the drawable the floating selection
   *  is attached to. if it is then we'll do the attachment at
   *  the end of this function.
   */
  add_floating_sel = (info->cp == info->floating_sel_offset);

  /* read in the layer width, height and name */
  info->cp += xcf_read_int32 (info->fp, (guint32*) &width, 1);
  info->cp += xcf_read_int32 (info->fp, (guint32*) &height, 1);
  info->cp += xcf_read_string (info->fp, &name, 1);

  /* create a new channel */
  channel = channel_new (gimage, width, height, name, 255, color);
  if (!channel)
    {
      g_free (name);
      return NULL;
    }

  /* read in the channel properties */
  if (!xcf_load_channel_props (info, gimage, channel))
    goto error;

  /* read the hierarchy and layer mask offsets */
  info->cp += xcf_read_int32 (info->fp, &hierarchy_offset, 1);

  /* read in the hierarchy */
  xcf_seek_pos (info, hierarchy_offset);
  if (!xcf_load_hierarchy (info, GIMP_DRAWABLE(channel)->tiles))
    goto error;

  /* attach the floating selection... */
  if (add_floating_sel)
    {
      Layer *floating_sel;

      floating_sel = info->floating_sel;
      floating_sel_attach (floating_sel, GIMP_DRAWABLE(channel));
    }

  return channel;

error:
  channel_delete (channel);
  return NULL;
}

static LayerMask*
xcf_load_layer_mask (XcfInfo *info,
		     GImage  *gimage)
{
  LayerMask *layer_mask;
  guint32 hierarchy_offset;
  int width;
  int height;
  int add_floating_sel;
  char *name;
  guchar color[3] = { 0, 0, 0 };

  /* check and see if this is the drawable the floating selection
   *  is attached to. if it is then we'll do the attachment at
   *  the end of this function.
   */
  add_floating_sel = (info->cp == info->floating_sel_offset);

  /* read in the layer width, height and name */
  info->cp += xcf_read_int32 (info->fp, (guint32*) &width, 1);
  info->cp += xcf_read_int32 (info->fp, (guint32*) &height, 1);
  info->cp += xcf_read_string (info->fp, &name, 1);

  /* create a new layer mask */
  layer_mask = layer_mask_new (gimage, width, height, name, 255, color);
  if (!layer_mask)
    {
      g_free (name);
      return NULL;
    }

  /* read in the layer_mask properties */
  if (!xcf_load_channel_props (info, gimage, GIMP_CHANNEL(layer_mask)))
    goto error;

  /* read the hierarchy and layer mask offsets */
  info->cp += xcf_read_int32 (info->fp, &hierarchy_offset, 1);

  /* read in the hierarchy */
  xcf_seek_pos (info, hierarchy_offset);
  if (!xcf_load_hierarchy (info, GIMP_DRAWABLE(layer_mask)->tiles))
    goto error;

  /* attach the floating selection... */
  if (add_floating_sel)
    {
      Layer *floating_sel;

      floating_sel = info->floating_sel;
      floating_sel_attach (floating_sel, GIMP_DRAWABLE(layer_mask));
    }

  return layer_mask;

error:
  layer_mask_delete (layer_mask);
  return NULL;
}

static gint
xcf_load_hierarchy (XcfInfo     *info,
		    TileManager *tiles)
{
  guint32 saved_pos;
  guint32 offset;
  guint32 junk;
  int width;
  int height;
  int bpp;

  info->cp += xcf_read_int32 (info->fp, (guint32*) &width, 1);
  info->cp += xcf_read_int32 (info->fp, (guint32*) &height, 1);
  info->cp += xcf_read_int32 (info->fp, (guint32*) &bpp, 1);

  /* make sure the values in the file correspond to the values
   *  calculated when the TileManager was created.
   */
  if ((width != tiles->width) ||
      (height != tiles->height) ||
      (bpp != tiles->bpp))
    return FALSE;

  /* load in the levels...we make sure that the number of levels
   *  calculated when the TileManager was created is the same
   *  as the number of levels found in the file.
   */

  info->cp += xcf_read_int32 (info->fp, &offset, 1); /* top level */

  /* discard offsets for layers below first, if any.
   */
  do 
    {
      info->cp += xcf_read_int32 (info->fp, &junk, 1);
    }
  while (junk != 0);

  /* save the current position as it is where the
   *  next level offset is stored.
   */
  saved_pos = info->cp;
  
  /* seek to the level offset */
  xcf_seek_pos (info, offset);
  
  /* read in the level */
  if (!xcf_load_level (info, tiles))
    return FALSE;
      
  /* restore the saved position so we'll be ready to
   *  read the next offset.
   */
  xcf_seek_pos (info, saved_pos);

  return TRUE;
}


static gint
xcf_load_level (XcfInfo     *info,
		TileManager *tiles)
{
  guint32 saved_pos;
  guint32 offset, offset2;
  guint ntiles;
  int width;
  int height;
  int i;
  int fail;
  Tile *previous;
  Tile *tile;

  info->cp += xcf_read_int32 (info->fp, (guint32*) &width, 1);
  info->cp += xcf_read_int32 (info->fp, (guint32*) &height, 1);

  if ((width != tiles->width) ||
      (height != tiles->height))
    return FALSE;

  /* read in the first tile offset.
   *  if it is '0', then this tile level is empty
   *  and we can simply return.
   */
  info->cp += xcf_read_int32 (info->fp, &offset, 1);
  if (offset == 0)
    return TRUE;

  /* Initialise the reference for the in-memory tile-compression
   */
  previous = NULL;

  ntiles = tiles->ntile_rows * tiles->ntile_cols;
  for (i = 0; i < ntiles; i++)
    {
      fail = FALSE;

      if (offset == 0)
	{
	  g_message (_("not enough tiles found in level"));
	  return FALSE;
	}

      /* save the current position as it is where the
       *  next tile offset is stored.
       */
      saved_pos = info->cp;

      /* read in the offset of the next tile so we can calculate the amount
	 of data needed for this tile*/
      info->cp += xcf_read_int32 (info->fp, &offset2, 1);
      
      /* if the offset is 0 then we need to read in the maximum possible
	 allowing for negative compression */
      if (offset2 == 0)
	offset2 = offset + TILE_WIDTH*TILE_WIDTH*4*1.5; /* 1.5 is probably more
							   than we need to allow */

      /* seek to the tile offset */
      xcf_seek_pos (info, offset);

      /* get the tile from the tile manager */
      tile = tile_manager_get (tiles, i, TRUE, TRUE);

      /* read in the tile */
      switch (info->compression)
	{
	case COMPRESS_NONE:
	  if (!xcf_load_tile (info, tile))
	    fail = TRUE;
	  break;
	case COMPRESS_RLE:
	  if (!xcf_load_tile_rle (info, tile, offset2 - offset))
	    fail = TRUE;
	  break;
	case COMPRESS_ZLIB:
	  g_error (_("xcf: zlib compression unimplemented"));
	  fail = TRUE;
	  break;
	case COMPRESS_FRACTAL:
	  g_error (_("xcf: fractal compression unimplemented"));
	  fail = TRUE;
	  break;
	}

      if (fail) 
	{
	  tile_release (tile, TRUE);
	  return FALSE;
	}
	    
      /* To potentially save memory, we compare the
       *  newly-fetched tile against the last one, and
       *  if they're the same we copy-on-write mirror one against
       *  the other.
       */
      if (previous != NULL) 
	{
	  tile_lock (previous);
	  if (tile_ewidth (tile) == tile_ewidth (previous) &&
	      tile_eheight (tile) == tile_eheight (previous) &&
	      tile_bpp (tile) == tile_bpp (previous) &&
	      memcmp (tile_data_pointer(tile, 0, 0), 
		      tile_data_pointer(previous, 0, 0),
		      tile_size (tile)) == 0)
	    tile_manager_map (tiles, i, previous);
	  tile_release (previous, FALSE);
	}
      tile_release (tile, TRUE);
      previous = tile_manager_get (tiles, i, FALSE, FALSE);

      /* restore the saved position so we'll be ready to
       *  read the next offset.
       */
      xcf_seek_pos (info, saved_pos);

      /* read in the offset of the next tile */
      info->cp += xcf_read_int32 (info->fp, &offset, 1);
    }

  if (offset != 0)
    {
      g_message ("encountered garbage after reading level: %d", offset);
      return FALSE;
    }

  return TRUE;
}

static gint
xcf_load_tile (XcfInfo *info,
	       Tile    *tile)
{
#ifdef SWAP_FROM_FILE

  if (!info->swap_num)
    {
      info->ref_count = g_new (int, 1);
      info->swap_num = tile_swap_add (info->filename, xcf_swap_func, info->ref_count);
    }

  tile->swap_num = info->swap_num;
  tile->swap_offset = info->cp;
  *info->ref_count += 1;

#else

  info->cp += xcf_read_int8 (info->fp, tile_data_pointer(tile, 0, 0), 
			     tile_size (tile));

#endif

  return TRUE;
}

static gint
xcf_load_tile_rle (XcfInfo *info,
		   Tile    *tile,
		   int     data_length)
{
  guchar *data;
  guchar val;
  int size;
  int count;
  int length;
  int bpp;
  int i, j;
  guchar *xcfdata, *xcfodata;
  
  data = tile_data_pointer (tile, 0, 0);
  bpp = tile_bpp (tile);
 
  xcfdata = xcfodata = g_malloc(data_length);

  /* we have to use fread instead of xcf_read_* because we may be
     reading past the end of the file here */
  info->cp += fread ((char*) xcfdata, sizeof (char), data_length, info->fp);  

  for (i = 0; i < bpp; i++)
    {
      data = (guchar*)tile_data_pointer (tile, 0, 0) + i;
      size = tile_ewidth (tile) * tile_eheight (tile);
      count = 0;

      while (size > 0)
	{
	  val = *xcfdata++;

	  length = val;
	  if (length >= 128)
	    {
	      length = 255 - (length - 1);
	      if (length == 128)
		{
		  length = (*xcfdata << 8) + xcfdata[1];
		  xcfdata += 2;
		}

	      count += length;
	      size -= length;

	      while (length-- > 0)
		{
		  *data = *xcfdata++;
		  data += bpp;
		}
	    }
	  else
	    {
	      length += 1;
	      if (length == 128)
		{
		  length = (*xcfdata << 8) + xcfdata[1];
		  xcfdata += 2;
		}

	      count += length;
	      size -= length;

              if (size < 0)
                g_message (_("xcf: uh oh! xcf rle tile loading error: %d"), count);

	      val = *xcfdata++;

	      for (j = 0; j < length; j++)
		{
		  *data = val;
		  data += bpp;
		}
	    }
	}
    }
  g_free(xcfodata);
  return TRUE;
}


#ifdef SWAP_FROM_FILE

static int
xcf_swap_func (int       fd,
	       Tile     *tile,
	       int       cmd,
	       gpointer  user_data)
{
  int bytes;
  int err;
  int nleft;
  int *ref_count;

  switch (cmd)
    {
    case SWAP_IN:
      lseek (fd, tile->swap_offset, SEEK_SET);

      bytes = tile_size (tile);
      tile_alloc (tile);

      nleft = bytes;
      while (nleft > 0)
	{
	  do {
	    err = read (fd, tile->data + bytes - nleft, nleft);
	  } while ((err == -1) && ((errno == EAGAIN) || (errno == EINTR)));

	  if (err <= 0)
	    {
	      g_message (_("unable to read tile data from xcf file: %d ( %d ) bytes read"), err, nleft);
	      return FALSE;
	    }

	  nleft -= err;
	}
      break;
    case SWAP_OUT:
    case SWAP_DELETE:
    case SWAP_COMPRESS:
      ref_count = user_data;
      *ref_count -= 1;
      if (*ref_count == 0)
	{
	  tile_swap_remove (tile->swap_num);
	  g_free (ref_count);
	}

      tile->swap_num = 1;
      tile->swap_offset = -1;

      return TRUE;
    }

  return FALSE;
}

#endif


static void
xcf_seek_pos (XcfInfo *info,
	      guint    pos)
{
  if (info->cp != pos)
    {
      info->cp = pos;
      fseek (info->fp, info->cp, SEEK_SET);
    }
}

static void
xcf_seek_end (XcfInfo *info)
{
  fseek (info->fp, 0, SEEK_END);
  info->cp = ftell (info->fp);
}


static guint
xcf_read_int32 (FILE     *fp,
		guint32  *data,
		gint      count)
{
  guint total;

  total = count;
  if (count > 0)
    {
      xcf_read_int8 (fp, (guint8*) data, count * 4);

      while (count--)
        {
          *data = g_ntohl (*data);
          data++;
        }
    }

  return total * 4;
}

static guint
xcf_read_float (FILE     *fp,
		gfloat   *data,
		gint      count)
{
  return (xcf_read_int32(fp, (guint32 *)((void *)data), count));
}

static guint
xcf_read_int8 (FILE     *fp,
	       guint8   *data,
	       gint      count)
{
  guint total;
  int bytes;

  total = count;
  while (count > 0)
    {
      bytes = fread ((char*) data, sizeof (char), count, fp);
      if (bytes <= 0) /* something bad happened */
        break;
      count -= bytes;
      data += bytes;
    }

  return total;
}

static guint
xcf_read_string (FILE     *fp,
		 gchar   **data,
		 gint      count)
{
  guint32 tmp;
  guint total;
  int i;

  total = 0;
  for (i = 0; i < count; i++)
    {
      total += xcf_read_int32 (fp, &tmp, 1);
      if (tmp > 0)
        {
          data[i] = g_new (gchar, tmp);
          total += xcf_read_int8 (fp, (guint8*) data[i], tmp);
        }
      else
        {
          data[i] = NULL;
        }
    }

  return total;
}

static guint
xcf_write_int32 (FILE     *fp,
		 guint32  *data,
		 gint      count)
{
  guint32 tmp;
  int i;

  if (count > 0)
    {
      for (i = 0; i < count; i++)
        {
          tmp = g_htonl (data[i]);
          xcf_write_int8 (fp, (guint8*) &tmp, 4);
        }
    }

  return count * 4;
}

static guint
xcf_write_float (FILE     *fp,
		 gfloat   *data,
		 gint      count)
{
  return (xcf_write_int32(fp, (guint32 *)((void *)data), count));
}

static guint
xcf_write_int8 (FILE     *fp,
		guint8   *data,
		gint      count)
{
  guint total;
  int bytes;

  total = count;
  while (count > 0)
    {
      bytes = fwrite ((char*) data, sizeof (char), count, fp);
      count -= bytes;
      data += bytes;
    }

  return total;
}

static guint
xcf_write_string (FILE     *fp,
		  gchar   **data,
		  gint      count)
{
  guint32 tmp;
  guint total;
  int i;

  total = 0;
  for (i = 0; i < count; i++)
    {
      if (data[i])
        tmp = strlen (data[i]) + 1;
      else
        tmp = 0;

      xcf_write_int32 (fp, &tmp, 1);
      if (tmp > 0)
        xcf_write_int8 (fp, (guint8*) data[i], tmp);

      total += 4 + tmp;
    }

  return total;
}
