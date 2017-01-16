/*
 * Open Firm Accounting
 * A double-entry accounting application for professional services.
 *
 * Copyright (C) 2014,2015,2016,2017 Pierre Wieser (see AUTHORS)
 *
 * Open Firm Accounting is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Open Firm Accounting is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Open Firm Accounting; see the file COPYING. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *   Pierre Wieser <pwieser@trychlos.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <archive.h>
#include <archive_entry.h>

#include "my/my-utils.h"

#include "api/ofa-hub.h"
#include "api/ofa-igetter.h"

#include "ui/ofa-application.h"
#include "ui/ofa-maintainer.h"

static void old_functions( void );
static void test_compressor( ofaIGetter *getter );
static void test_libarchive( ofaIGetter *getter );

/**
 * ofa_maintainer_run_by_application:
 */
void
ofa_maintainer_run_by_application( ofaApplication *application )
{
	static const gchar *thisfn = "ofa_maintainer_run_by_application";

	g_debug( "%s: application=%p", thisfn, ( void * ) application );

	if( 0 ){
		old_functions();
		test_compressor( OFA_IGETTER( application ));
		test_libarchive( OFA_IGETTER( application ));
	}
}

static void
old_functions( void )
{
#if 0
	gchar *cstr = "GRANT ALL PRIVILEGES ON `ofat`.* TO 'ofat'@'localhost' WITH GRANT OPTION";
	gchar *prev_dbname = "ofat";
	gchar *dbname = "ofat_3";
	GRegex *regex;
	gchar *str = g_strdup_printf( " `%s`\\.\\* ", prev_dbname );
	g_debug( "%s: str='%s'", thisfn, str );
	regex = g_regex_new( str, 0, 0, NULL );
	g_free( str );
	/*str = g_strdup_printf( "\\1%s", dbname );*/
	str = g_strdup_printf( " `%s`.* ", dbname );
	g_debug( "%s: str=%s", thisfn, str );
	if( g_regex_match( regex, cstr, 0, NULL )){
		gchar *query = g_regex_replace( regex, cstr, -1, 0, str, 0, NULL );
		g_debug( "%s: cstr=%s", thisfn, cstr );
		g_debug( "%s: query=%s", thisfn, query );
	}

	/* test formula engine */
	if( 0 ){
		ofa_formula_test( priv->hub );
	}

	/* generate 100 pseudo random integers */
	guint i;
	gchar *key;

	for( i=1 ; i<100 ; ++i ){
		key = g_strdup_printf( "%8x", g_random_int());
		g_debug( "%s: i=%u, key=%s", thisfn, i, key );
		g_free( key );
	}
#endif
}

/*
 * As a remainder: GZlibCompressor provides GZIP and ZLIB formats
 * But none of them has a tar-like feature of supporting several
 * source files into a single compressed file.
 * So this is definitively not what we want.
 */
static void
test_compressor( ofaIGetter *getter )
{
#if 0
	static const gchar *thisfn = "ofa_maintainer_test_compressor";
	static const gchar *uri = "file:///tmp/test-compressor.gz";
	static const gchar *text = "Cuius acerbitati uxor grave accesserat incentivum, germanitate Augusti turgida supra modum, quam Hannibaliano regi fratris filio antehac Constantinus iunxerat pater, Megaera quaedam mortalis, inflammatrix saevientis adsidua, humani cruoris avida nihil mitius quam maritus; qui paulatim eruditiores facti processu temporis ad nocendum per clandestinos versutosque rumigerulos conpertis leviter addere quaedam male suetos falsa et placentia sibi discentes, adfectati regni vel artium nefandarum calumnias insontibus adfligebant.";
	GFile *file = g_file_new_for_uri( uri );
	GError *error = NULL;
	GFileOutputStream *output_stream = g_file_replace( file, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, NULL, &error );
	if( !output_stream ){
		g_warning( "%s: %s", thisfn, error->message );
		g_error_free( error );
		return;
	}
	GZlibCompressor *compressor = g_zlib_compressor_new( G_ZLIB_COMPRESSOR_FORMAT_GZIP, -1 );
	GFileInfo *info = g_file_info_new();
	g_file_info_set_name( info, "Text1" );
	g_zlib_compressor_set_file_info( compressor, info );
	g_object_unref( info );

	gsize insize = my_strlen( text );
	gsize outsize = 2*insize;					// hope this is enough
	gchar *outbuf = g_new0( gchar, outsize );
	gboolean finished = FALSE;
	gboolean abort = FALSE;
	gsize bytes_read, bytes_written;

	while( !finished && !abort ){
		GConverterResult result = g_converter_convert( G_CONVERTER( compressor ),
					text, insize, outbuf, outsize, G_CONVERTER_INPUT_AT_END, &bytes_read, &bytes_written, &error );
		/* when writing the json header
		 * result=G_CONVERTER_FINISHED, read=516, written=273 */
		switch( result ){
			case G_CONVERTER_ERROR:
				g_warning( "%s: result=G_CONVERTER_ERROR, error=%s", thisfn, error->message );
				g_error_free( error );
				abort = TRUE;
				break;
			case G_CONVERTER_CONVERTED:
			case G_CONVERTER_FINISHED:
				if( result == G_CONVERTER_CONVERTED ){
					g_debug( "%s: result=G_CONVERTER_CONVERTED, read=%lu, written=%lu", thisfn, bytes_read, bytes_written );
				} else {
					g_debug( "%s: result=G_CONVERTER_FINISHED, read=%lu, written=%lu", thisfn, bytes_read, bytes_written );
				}
				outsize = bytes_written;
				if( !g_output_stream_write_all( G_OUTPUT_STREAM( output_stream ), outbuf, outsize, &bytes_written, NULL, &error )){
					g_warning( "%s: error=%s", thisfn, error->message );
					g_error_free( error );
					abort = TRUE;
				} else {
					g_debug( "%s: %lu bytes written to output stream", thisfn, bytes_written );
				}
				if( result == G_CONVERTER_FINISHED ){
					finished = TRUE;
				}
				break;
			case G_CONVERTER_FLUSHED:
				g_debug( "%s: result=G_CONVERTER_FLUSHED, read=%lu, written=%lu", thisfn, bytes_read, bytes_written );
				finished = TRUE;
				break;
		}
	}

	g_output_stream_close( G_OUTPUT_STREAM( output_stream ), NULL, NULL );
	g_object_unref( output_stream );
	g_free( outbuf );
	g_object_unref( compressor );

	/* a new compressor object, even with a new name, still writes its
	 * output data to the same output logical stream of output .gz file
	 */
	compressor = g_zlib_compressor_new( G_ZLIB_COMPRESSOR_FORMAT_GZIP, -1 );
	info = g_file_info_new();
	g_file_info_set_name( info, "Text2" );
	g_zlib_compressor_set_file_info( compressor, info );
	g_object_unref( info );

	output_stream = g_file_append_to( file, G_FILE_CREATE_NONE, NULL, &error );
	if( !output_stream ){
		g_warning( "%s: %s", thisfn, error->message );
		g_error_free( error );
		return;
	}

	insize = my_strlen( text );
	outsize = 2*insize;					// hope this is enough
	outbuf = g_new0( gchar, outsize );
	finished = FALSE;
	abort = FALSE;

	while( !finished && !abort ){
		GConverterResult result = g_converter_convert( G_CONVERTER( compressor ),
					text, insize, outbuf, outsize, G_CONVERTER_INPUT_AT_END, &bytes_read, &bytes_written, &error );
		/* when writing the json header
		 * result=G_CONVERTER_FINISHED, read=516, written=273 */
		switch( result ){
			case G_CONVERTER_ERROR:
				g_warning( "%s: result=G_CONVERTER_ERROR, error=%s", thisfn, error->message );
				g_error_free( error );
				abort = TRUE;
				break;
			case G_CONVERTER_CONVERTED:
			case G_CONVERTER_FINISHED:
				if( result == G_CONVERTER_CONVERTED ){
					g_debug( "%s: result=G_CONVERTER_CONVERTED, read=%lu, written=%lu", thisfn, bytes_read, bytes_written );
				} else {
					g_debug( "%s: result=G_CONVERTER_FINISHED, read=%lu, written=%lu", thisfn, bytes_read, bytes_written );
				}
				outsize = bytes_written;
				if( !g_output_stream_write_all( G_OUTPUT_STREAM( output_stream ), outbuf, outsize, &bytes_written, NULL, &error )){
					g_warning( "%s: error=%s", thisfn, error->message );
					g_error_free( error );
					abort = TRUE;
				} else {
					g_debug( "%s: %lu bytes written to output stream", thisfn, bytes_written );
				}
				if( result == G_CONVERTER_FINISHED ){
					finished = TRUE;
				}
				break;
			case G_CONVERTER_FLUSHED:
				g_debug( "%s: result=G_CONVERTER_FLUSHED, read=%lu, written=%lu", thisfn, bytes_read, bytes_written );
				finished = TRUE;
				break;
		}
	}

	g_output_stream_close( G_OUTPUT_STREAM( output_stream ), NULL, NULL );
	g_object_unref( output_stream );
	g_free( outbuf );
	g_object_unref( compressor );

	g_object_unref( file );
#endif
}

static void
test_libarchive( ofaIGetter *getter )
{
	static const gchar *thisfn = "ofa_maintainer_test_libarchive";
	static const gchar *uri = "file:///tmp/test-libarchive.zip";
	static const gchar *text1 = "Cuius acerbitati uxor grave accesserat incentivum, germanitate Augusti turgida supra modum, quam Hannibaliano regi fratris filio antehac Constantinus iunxerat pater, Megaera quaedam mortalis, inflammatrix saevientis adsidua, humani cruoris avida nihil mitius quam maritus; qui paulatim eruditiores facti processu temporis ad nocendum per clandestinos versutosque rumigerulos conpertis leviter addere quaedam male suetos falsa et placentia sibi discentes, adfectati regni vel artium nefandarum calumnias insontibus adfligebant.";
	static const gchar *text2 = "Primae exulque Baeticae dies Baeticae dies praefecto sperabatur consilio flammam per per duci funesti iussus aiunt comitatum manu cecidit Baeticae funesti exulque de exulque per dies de provocavit codicem lanuginis traditus manu de provocavit ut ad fumo aiunt comitatum patris duci firmato inpulsu manu aetatem convictus manu principem lanuginis duci mittendus comitatum Maximino artium exploratius consulari descripsisse patris firmato in Phalangio Phalangio codicem Lollianus adulescens Baeticae praefecto Lollianus sperabatur primae aetatem dies carnificis de patris in primae mittendus praefecto patris ut et adulescens Baeticae hos Phalangio de eius comitatum sperabatur iussus codicem consulari carnificis dies ad per lanuginis Baeticae eius.";

	struct archive *a;
	gchar *path;
	struct archive_entry *entry;
	//struct stat st;
	//char buff[8192];
	//int len;
	//int fd;
	gsize size, written;
	gint res;

	/* tar.gz format does not support not setting the size in the header
	 * so get stick with zip */

	GFile *file = g_file_new_for_uri( uri );
	path = g_file_get_path( file );

	a = archive_write_new();
	//archive_write_add_filter_gzip(a);
	//archive_write_set_format_pax_restricted(a); // Note 1
	archive_write_set_format_zip( a );
	archive_write_open_filename(a, path );

    entry = archive_entry_new(); // Note 2

    size = my_strlen( text1 );
    archive_entry_set_pathname( entry, "JSON_Header" );
    //archive_entry_set_size(entry, size); // Note 3 - optional
    archive_entry_set_filetype(entry, AE_IFREG);
    res = archive_write_header(a, entry);
    if( res != ARCHIVE_OK ){
    	g_warning( "%s: archive_write_header: %s", thisfn, archive_error_string( a ));
    } else {
    	written = archive_write_data( a, text1, size );
		if( written < 0 ){
			g_debug( "%s: archive_write_data: %s", thisfn, archive_error_string( a ));
		} else {
		    g_debug( "%s: text1: size=%lu, written=%lu", thisfn, size, written );
		    archive_write_finish_entry( a );
		}
    }

    archive_entry_clear( entry );
    /* for this second entry, writes the header after the data - doesn't work */
    /* write the header first, but set size after - not ok in zip format */
    /* not setting the size at all is ok in zip format */
    /* so size is not mandatory, and is automatically set */
    size = my_strlen( text2 );
    archive_entry_set_pathname( entry, "Text2" );
    archive_entry_set_filetype(entry, AE_IFREG);		// mandatory
    //archive_entry_set_size(entry, size); // Note 3
    res = archive_write_header(a, entry);
    if( res != ARCHIVE_OK ){
    	g_warning( "%s: archive_write_header: %s", thisfn, archive_error_string( a ));
    } else {
    	written = archive_write_data( a, text2, size );
		if( written < 0 ){
			g_debug( "%s: archive_write_data: %s", thisfn, archive_error_string( a ));
		} else {
			written += archive_write_data( a, text2, size );
			if( written < 0 ){
				g_debug( "%s: archive_write_data: %s", thisfn, archive_error_string( a ));
			} else {
				g_debug( "%s: text2: size=%lu, written=%lu", thisfn, 2*size, written );
				archive_write_finish_entry( a );
			}
		}
    }

    /*
	  while (*filename) {
	    stat(*filename, &st);
	    entry = archive_entry_new(); // Note 2
	    archive_entry_set_pathname(entry, *filename);
	    archive_entry_set_size(entry, st.st_size); // Note 3
	    archive_entry_set_filetype(entry, AE_IFREG);
	    archive_entry_set_perm(entry, 0644);
	    archive_write_header(a, entry);
	    fd = open(*filename, O_RDONLY);
	    len = read(fd, buff, sizeof(buff));
	    while ( len > 0 ) {
	        archive_write_data(a, buff, len);
	        len = read(fd, buff, sizeof(buff));
	    }
	    close(fd);
	    archive_entry_free(entry);
	    filename++;
	  }
	  */

    archive_entry_free( entry );

	archive_write_close(a); // Note 4
	archive_write_free(a); // Note 5
	g_free( path );
	g_object_unref( file );
}
