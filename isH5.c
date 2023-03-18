/*! @file isH5.c
 *  @copyright 2017 by Northwestern University
 *  @author Keith Brister
 *  @brief Routines to support reading hdf5 files generated by the Dectris Eiger detector
 */
#include "is.h"

/** Frames found when searching the data files.  Each data file is
 ** queried to find its first and last frame numbers so we know which
 ** file to open when looking for a particular frame number.  BTW,
 ** frame numbers start at 1.
 */
typedef struct frame_discovery_struct {
  struct frame_discovery_struct *next;  //!< The index frame_discovery_struct in our list
  hid_t data_set;                       //!< our h5 dataset
  hid_t file_space;                     //!< the file space
  hid_t file_type;                      //!< the file type, of course
  int32_t first_frame;                  //!< first frame number in this dataset
  int32_t last_frame;                   //!< last frame number in this dataset
  char *done_list;                      //!< List of frames we've processed already.  Not yet used in this project.
} frame_discovery_t;

/** Extra information. We need to keep track of so we don't have to
 ** recalculate it for the next query.
 */
typedef struct isH5extraStruct {
  frame_discovery_t *frame_discovery_base;   //!< List of discovered frames
} isH5extra_t;

/** h5 to json equivalencies.  We read HDF5 properties and convert
 ** them to json to use and/or transmit back to the user's browser.
 */
typedef struct h5_to_json_struct {
  char *h5_location;                    //!< HDF5 property name
  char *json_property_name;             //!< JSON equivalent
  char type;                            //!< i=int, f=float, s=string, F=float array
} h5_to_json_t;

/** Our mapping between hdf5 file properties and our metadata object properties for 
 */
h5_to_json_t json_convert_array[] = {
  /*Params that have been around since the beginning. If it is commented out, it was removed in the Eiger 2X 16M.*/
  { "/entry/instrument/detector/detectorSpecific/auto_summation",                  "auto_summation",                                  'i'},
  { "/entry/instrument/detector/beam_center_x",                                    "beam_center_x",                                   'f'},
  { "/entry/instrument/detector/beam_center_y",                                    "beam_center_y",                                   'f'},
  { "/entry/instrument/detector/bit_depth_readout",                                "bit_depth_readout",                               'i'},
  { "/entry/instrument/detector/bit_depth_image",                                  "bit_depth_image",                                 'i'},
  { "/entry/instrument/detector/detectorSpecific/calibration_type",                "calibration_type",                                's'},/* pre-1.8.0 */
  { "/entry/sample/goniometer/chi_increment",                                      "chi_increment",                                   'f'},/* pre-1.8.0 */
  { "/entry/sample/goniometer/chi_start",                                          "chi_start",                                       'f'},/* pre-1.8.0 */
  { "/entry/instrument/detector/count_time",                                       "count_time",                                      'f'},
  { "/entry/instrument/detector/detectorSpecific/countrate_correction_bunch_mode", "countrate_correction_bunch_mode",                 's'},/* pre-1.8.0 */
  { "/entry/instrument/detector/detectorSpecific/data_collection_date",            "data_collection_date",                            's'},
  { "/entry/instrument/detector/description",                                      "description",                                     's'},
  { "/entry/instrument/detector/detector_distance",                                "detector_distance",                               'f'},
  { "/entry/instrument/detector/detector_number",                                  "detector_number",                                 's'},
  { "/entry/instrument/detector/geometry/orientation/value",                       "detector_orientation",                            'F'},
  { "/entry/instrument/detector/detectorSpecific/detector_readout_period",         "detector_readout_period",                         'f'},/* pre-1.8.0 */
  { "/entry/instrument/detector/detector_readout_time",                            "detector_readout_time",                           'f'},
  { "/entry/instrument/detector/geometry/translation/distances",                   "detector_translation",                            'F'},
  { "/entry/instrument/detector/efficiency_correction_applied",                    "efficiency_correction_applied",                   'i'},/* pre-1.8.0 */
  { "/entry/instrument/detector/detectorSpecific/element",                         "element",                                         's'},
  { "/entry/instrument/detector/flatfield_correction_applied",                     "flatfield_correction_applied",                    'i'},
  { "/entry/instrument/detector/detectorSpecific/frame_count_time",                "frame_count_time",                                'f'},
  { "/entry/instrument/detector/detectorSpecific/frame_period",                    "frame_period",                                    'f'},
  { "/entry/instrument/detector/frame_time",                                       "frame_time",                                      'f'},
  { "/entry/sample/goniometer/kappa_increment",                                    "kappa_increment",                                 'f'},/* pre-1.8.0 */
  { "/entry/sample/goniometer/kappa_start",                                        "kappa_start",                                     'f'},
  { "/entry/instrument/detector/detectorSpecific/nframes_sum",                     "nframes_sum",                                     'i'},/* pre-1.8.0 */
  { "/entry/instrument/detector/detectorSpecific/nimages",                         "nimages",                                         'i'},
  { "/entry/instrument/detector/detectorSpecific/ntrigger",                        "ntrigger",                                        'i'},
  { "/entry/instrument/detector/detectorSpecific/number_of_excluded_pixels",       "number_of_excluded_pixels",                       'i'},
  { "/entry/sample/goniometer/omega_increment",                                    "omega_increment",                                 'i'},/* pre-1.8.0 */
  { "/entry/sample/goniometer/omega_start",                                        "omega_start",                                     'f'},/* pre-1.8.0 */
  { "/entry/sample/goniometer/phi_increment",                                      "phi_increment",                                   'f'},/* pre-1.8.0 */
  { "/entry/sample/goniometer/phi_start",                                          "phi_start",                                       'f'},/* pre-1.8.0 */
  { "/entry/instrument/detector/detectorSpecific/photon_energy",                   "photon_energy",                                   'f'},
  { "/entry/instrument/detector/pixel_mask_applied",                               "pixel_mask_applied",                              'i'},
  { "/entry/instrument/detector/sensor_material",                                  "sensor_material",                                 's'},
  { "/entry/instrument/detector/sensor_thickness",                                 "sensor_thickness",                                'f'},
  { "/entry/instrument/detector/detectorSpecific/software_version",                "software_version",                                's'},
  { "/entry/instrument/detector/detectorSpecific/summation_nimages",               "summation_nimages",                               'i'},/* pre-1.8.0 */
  { "/entry/instrument/detector/threshold_energy",                                 "threshold_energy",                                'f'},
  { "/entry/instrument/detector/detectorSpecific/trigger_mode",                    "trigger_mode",                                    's'},
  { "/entry/instrument/detector/goniometer/two_theta_increment",                   "two_theta_increment",                             'f'},/* pre-1.8.0 */
  { "/entry/instrument/detector/goniometer/two_theta_start",                       "two_theta_start",                                 'f'},
  { "/entry/instrument/detector/virtual_pixel_correction_applied",                 "virtual_pixel_correction_applied",                'i'},
  { "/entry/instrument/beam/incident_wavelength",                                  "wavelength",                                      'f'},
  { "/entry/instrument/detector/x_pixel_size",                                     "x_pixel_size",                                    'f'},
  { "/entry/instrument/detector/detectorSpecific/x_pixels_in_detector",            "x_pixels_in_detector",                            'i'},
  { "/entry/instrument/detector/y_pixel_size",                                     "y_pixel_size",                                    'f'},
  { "/entry/instrument/detector/detectorSpecific/y_pixels_in_detector",            "y_pixels_in_detector",                            'i'},
};

/** Get a hdf5 property as a JSON object.
 **
 ** @param[in]  master_file  Our open master file
 **
 ** @param[in]  htj          names of h5 and json objects with their type
 **
 ** @returns JSON object or NULL on a problem with the data file.
 **
 ** @remark Be sure to call json_decref on the returned object when
 ** done.  Programming errors are fatal with a brief but descriptive
 ** unique message.
 **/
json_t *get_json( const char *fn, hid_t master_file, h5_to_json_t *htj) {
  static const char *id = FILEID "get_json";
  json_t *rtn;          // our returned object
  herr_t herr;          // h5 error
  hid_t data_set;       // our data set
  hid_t data_type;      // data type
  hid_t data_space;     // data space
  hid_t mem_type;       // in memory type
  int32_t i_value;      // integer value
  float   f_value;      // float value
  float  *fa_value;     // float array value
  char   *s_value;      // string value
  hsize_t value_length; // length of string to read
  hsize_t *dims;        // current size of each array dimension
  hsize_t npoints;      // total number of array elements
  int rank;             // rank of the array to read
  int failed;           // 0 = AOK, 1 = Failure
  
  failed = 0;
  rtn = json_object();
  if (rtn == NULL) {
    isLogging_err("%s: Failed to create return object\n", id);
    exit (-1);
  }

  //isLogging_debug("%s: Calling H5Dopen2 for %s", id, fn);
  data_set = H5Dopen2( master_file, htj->h5_location, H5P_DEFAULT);
  if (data_set < 0) {
    isLogging_err("%s: Could not open data_set %s\n", id, htj->h5_location);
    json_decref(rtn);
    return NULL;
  }

  switch(htj->type) {
  case 'i':
    herr = H5Dread(data_set, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &i_value);
    if (herr < 0) {
      isLogging_err("%s: Could not read %s\n", id, htj->h5_location);
      failed = 1;
      break;
    }
    set_json_object_integer(id, rtn, htj->json_property_name, i_value);
    break;
      
  case 'f':
    herr = H5Dread(data_set, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &f_value);
    if (herr < 0) {
      isLogging_err("%s: Could not read %s\n", id, htj->h5_location);
      failed = 1;
      break;
    }
    set_json_object_real(id, rtn, htj->json_property_name, f_value);
    break;
      
  case 's':
    data_type = H5Dget_type(data_set);
    if (data_type < 0) {
      isLogging_err("%s: Could not get data_type (%s)\n", id, htj->h5_location);
      failed = 1;
      break;
    }
    value_length = H5Tget_size(data_type);
      
    if (value_length < 0) {
      isLogging_err("%s: Could not determine length of string for dataset %s\n", id, htj->h5_location);
      failed = 1;
      break;
    }
    s_value = calloc(value_length+1, 1);
    if (s_value == NULL) {
      isLogging_crit("%s: Out of memory (s_value)\n", id);
      failed = 1;
      break;
    }
      
    mem_type = H5Tcopy(H5T_C_S1);
    if (mem_type < 0) {
      isLogging_err("%s: Could not copy type for %s\n", id, htj->h5_location);
      failed = 1;
      break;
    }
      
    herr = H5Tset_size(mem_type, value_length);
    if (herr < 0) {
      isLogging_err("%s: Could not set memory type size (%s)\n", id, htj->h5_location);
      failed = 1;
      break;
    }
      
    herr = H5Dread(data_set, mem_type, H5S_ALL, H5S_ALL, H5P_DEFAULT, s_value);
    if (herr < 0) {
      isLogging_err("%s: Could not read %s\n", id, htj->h5_location);
      failed = 1;
      break;
    }
      
    set_json_object_string(id, rtn, htj->json_property_name, s_value);
      
    herr = H5Tclose(data_type);
    if (herr < 0) {
      isLogging_err("%s: Could not close data_type (s_value)\n", id);
      failed = 1;
      break;
    }
      
    herr = H5Tclose(mem_type);
    if (herr < 0) {
      isLogging_err("%s: Could not close mem_type (s_value)\n", id);
      failed = 1;
      break;
    }
    free(s_value);
    s_value = NULL;
    break;
      
  case 'F':
    // Here we assume we know the array length ahead of time.  Probably a bad assumption.
      
    data_space = H5Dget_space(data_set);
    if (data_space < 0) {
      isLogging_err("%s: Could not get data_space (float array)\n", id);
      failed = 1;
      break;
    }
      
    rank = H5Sget_simple_extent_ndims(data_space);
    if (rank < 0) {
      isLogging_err("%s: Could not get rank of data space (float array)\n", id);
      failed = 1;
      break;
    }
      
    dims = calloc(rank, sizeof(*dims));
    if (dims == NULL) {
      isLogging_err("%s: Could not allocate memory for dims array (float array)\n", id);
      failed = 1;
      break;
    }
      
    herr = H5Sget_simple_extent_dims(data_space, dims, NULL);
    if (herr < 0) {
      isLogging_err("%s: Could not get dimensions of float array\n", id);
      failed = 1;
      break;
    }
      
    npoints = H5Sget_simple_extent_npoints(data_space);
    if (npoints < 0) {
      isLogging_err("%s: Failed to get number of elements for float array\n", id);
      failed = 1;
      break;
    }
      
    fa_value = calloc( npoints, sizeof(float));
    if (fa_value == NULL) {
      isLogging_crit("%s: Out of memory (fa_value)\n", id);
      failed = 1;
      break;
    }
      
    herr = H5Dread(data_set, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, fa_value);
    if (herr < 0) {
      isLogging_err("%s: Could not read %d float values from %s\n", id, (int)npoints, htj->h5_location);
      failed = 1;
      break;
    }
      
    switch (rank) {
    case 1:
      set_json_object_float_array(id, rtn, htj->json_property_name, fa_value, dims[0]);
      break;
    case 2:
      set_json_object_float_array_2d(id, rtn, htj->json_property_name, fa_value, dims[1], dims[0]);
      break;
        
    default:
      isLogging_err("%s: Unsupported json array rank (%d)\n", id, rank);
      failed = 1;
      break;
    }
      
    H5Sclose(data_space);
    free(dims);
    free(fa_value);
    fa_value = NULL;
    break;
      
  default:
    isLogging_err("%s: data_set type code %c not implemented (%s)\n", id, htj->type, htj->h5_location);
    failed = 1;
    break;
  }

  herr = H5Dclose(data_set);
  if (herr < 0) {
    isLogging_err("%s: could not close dataset for %s\n", id, htj->h5_location);
    failed = 1;
  }

  if (failed) {
    json_decref(rtn);
    return NULL;
  }

  return rtn;
}

/** Read the meta data from a file.
 **
 ** @param[in] fn Name of the master file to open
 **
 ** @returns JSON object contianing the metadata.  json_decref must be
 ** called when you are done with it.  Null is returned on an error
 ** with the file.
 **
 ** @remark Programming errors are fatal.
 */
json_t *isH5GetMeta(isWorkerContext_t *wctx, const char *fn) {
  static const char *id = FILEID "isH5Jpeg";
  hid_t master_file;            // master file object
  herr_t herr;                  // error from an h5 call
  json_t *meta;                 // our meta data return object
  json_t *tmp_obj;              // temporary json object used to create meta
  int i;                        // loop over json conversion array
  int err;                      // error code for procedures that like to return ints.
  
  json_t* dcu_version;          // Eiger DCU software version
  const struct h5_json_property* properties = NULL;
  int n_properties = -1;
  
  
  //
  // Open up the master file
  //
  master_file = H5Fopen(fn, H5F_ACC_RDONLY, H5P_DEFAULT);
  if (master_file < 0) {
    isLogging_err("%s: Could not open master file %s\n", id, fn);
    return NULL;
  }

  //
  // Find the meta data
  //
  meta = json_object();
  if (meta == NULL) {
    isLogging_err("%s: Could not create metadata object in file %s\n", id, fn);

    herr = H5Fclose(master_file);
    if (herr < 0) {
      isLogging_err("%s: failed to close master file\n", id);
    }
    return NULL;
  }

  pthread_mutex_lock(&wctx->metaMutex);
  //
  // Get the software version and the associated properties to convert.
  //
  dcu_version = get_dcu_version(master_file);
  if (dcu_version == NULL) {
    isLogging_err("%s: failed to get DCU version %s\n", id, fn);
  }
  
  const char* dcu_version_str = json_string_value( json_object_get(dcu_version, json_convert_software_version.json_name) );
  if (strcmp("1.8.0", dcu_version_str) == 0) {
    properties = json_convert_array_1_8;
    n_properties = json_convert_array_1_8_size;
  } else {
    properties = json_convert_array_1_6;
    n_properties = json_convert_array_1_6_size;
  }
  
  for (i=0; i < n_properties; i++) {
    tmp_obj = h5_property_to_json(master_file, &properties[i]);
    
    if (!tmp_obj) {
      continue; // Some variables were added/removed in newer versions, no problem.
    }

    err = json_object_update(meta, tmp_obj);
    if (err != 0) {
      isLogging_err("%s: Could not update meta_obj\n", id);
      
      herr = H5Fclose(master_file);
      if (herr < 0) {
        isLogging_err("%s: failed to close master file\n", id);
      }
      json_decref(meta);
      json_decref(tmp_obj);
      pthread_mutex_unlock(&wctx->metaMutex);
      return NULL;
    }
    json_decref(tmp_obj);
  }

  set_json_object_integer(id, meta, "image_depth",  json_integer_value(json_object_get(meta,"bit_depth_image"))/8);

  herr = H5Fclose(master_file);
  if (herr < 0) {
    isLogging_err("%s: failed to close master file\n", id);
  }

  set_json_object_string(id, meta, "fn", fn);

  pthread_mutex_unlock(&wctx->metaMutex);

  return meta;
}

/** Callback for H5Lvisit_by_name
 **
 ** @param[in] lid       hdf5 link idenifier
 **
 ** @param[in] name      name of the hdf5 property relative to 'lid'
 **
 ** @param[in] info      description of the link
 **
 ** @param[in] op_data   Pointer to our list of discovered frames
 **
 ** @returns -1 on failure, 0 on success (keep going), 1 on success
 ** (stop since we found what we are looking for)
 **  
 */
int discovery_cb(hid_t lid, const char *name, const H5L_info_t *info, void *op_data) {
  static const char *id = FILEID "discovery_cb";
  isH5extra_t *extra;                   // cast op_data into something useful
  /*
  char s[256];                          // retrieve string from to parse file name and 
  const char *fnp;                      // file name pointer
  const char *pp;                       // path name pointer
  */
  herr_t herr;                          // h5 error code
  hid_t image_nr_high;                  // largest frame number number in this file
  hid_t image_nr_low;                   // smallest frame number in this file
  frame_discovery_t *these_frames,      // current entry in our list for discovered frames
    *fp, *fpp;                          // used to walk the frame_discovery linked list
  int failed;                           // 0 = AOK, 1 = success but don't go on, -1 = failed

  extra = op_data;
  failed = 0;

  these_frames = calloc(sizeof(frame_discovery_t), 1);
  if (these_frames == NULL) {
    isLogging_crit("%s: Out of memory (these_frames)\n", id);
    exit (-1);
  }

  //
  // setting member "next" is a bit of trouble since we want to keep
  // the data in the same order that we were called.
  //

  // Find the last non-null frame discovery pointer
  fpp = NULL;
  for (fp = extra->frame_discovery_base; fp != NULL; fp = fp->next) {
    fpp = fp;
  }

  // Initialize these_frames
  these_frames->next = NULL;
  if (fpp == NULL) {
    extra->frame_discovery_base = these_frames;
  } else {
    fpp->next = these_frames;
  }

  //
  // Error Breakout Box: set failed to one and break to perform cleanup and return.
  //
  do {
    /*
    if (info->type == H5L_TYPE_EXTERNAL) {
      herr = H5Lget_val(lid, name, s, sizeof(s), H5P_DEFAULT);
      if (herr < 0) {
        isLogging_err("%s: Could not get link value %s\n", id, name);
        failed = 1;
        break;
      }

      herr = H5Lunpack_elink_val(s, sizeof(s), 0, &fnp, &pp);
      if (herr < 0) {
        isLogging_err("%s: Could not unpack link value for %s\n", id, name);
        failed = 1;
        break;
      }    
    }
    */
    //isLogging_debug("%s: calling H5Dopen2 for %s", id, name);
    these_frames->data_set = H5Dopen2(lid, name, H5P_DEFAULT);
    if (these_frames->data_set < 0) {
      isLogging_err("%s: Failed to open dataset %s\n", id, name);
      failed = 1;
      break;
    }

    //isLogging_debug("%s: calling H5Dget_type for %s", id, name);
    these_frames->file_type = H5Dget_type(these_frames->data_set);
    if (these_frames->file_type < 0) {
      isLogging_err("%s: Could not get data_set type for %s\n", id, name);
      failed = 1;
      break;
    }

    //isLogging_debug("%s: calling H5Dget_space for %s", id, name);
    these_frames->file_space = H5Dget_space(these_frames->data_set);
    if (these_frames->file_space < 0) {
      isLogging_err("%s: Could not get data_set space for %s\n", id, name);
      failed = 1;
      break;
    }

    image_nr_high = H5Aopen_by_name( lid, name, "image_nr_high", H5P_DEFAULT, H5P_DEFAULT);
    if (image_nr_high < 0) {
      isLogging_err("%s: Could not open attribute 'image_nr_high' in linked file %s\n", id, name);
      failed = 1;
      break;
    }
  
    herr = H5Aread(image_nr_high, H5T_NATIVE_INT, &(these_frames->last_frame));
    if (herr < 0) {
      isLogging_err("%s: Could not read value 'image_nr_high' in linked file %s\n", id, name);
      failed = 1;
      break;
    }

    herr = H5Aclose(image_nr_high);
    if (herr < 0) {
      isLogging_err("%s: Failed to close attribute image_nr_high\n", id);
      failed = 1;
      break;
    }

    image_nr_low = H5Aopen_by_name( lid, name, "image_nr_low", H5P_DEFAULT, H5P_DEFAULT);
    if (image_nr_low < 0) {
      isLogging_err("%s: Could not open attribute 'image_nr_low' in linked file %s\n", id, name);
      failed = 1;
      break;
    }
  
    herr = H5Aread(image_nr_low, H5T_NATIVE_INT, &(these_frames->first_frame));
    if (herr < 0) {
      isLogging_err("%s: Could not read value 'image_nr_low' in linked file %s\n", id, name);
      failed = 1;
      break;
    }

    herr = H5Aclose(image_nr_low);
    if (herr < 0) {
      isLogging_err("%s: Failed to close attribute image_nr_low\n", id);
      failed = 1;
      break;
    }

    these_frames->done_list = calloc( these_frames->last_frame - these_frames->first_frame + 1, 1);
    if (these_frames->done_list == NULL) {
      isLogging_crit("%s: Out of memory (done_list)\n", id);
      failed = 1;
      break;
    }
  } while (0);

  if (failed) {
    return -1;
  }
  return 0;
}

/** Find a single frame in the named file.
 **
 ** @param[in,out] imb frame buffer to place our info in
 **
 ** @returns 0 on success, non-zero otherwise
 **
 */
int get_one_frame(isImageBufType **imbp) {
  static const char *id = FILEID "get_one_frame";
  isH5extra_t *extra;           // where we stored our frame discovery results
  frame_discovery_t *fp;        // Speaking of the devil
  int rank;                     // number of data dimensions (it had better be three)
  herr_t herr;                  // h5 error code
  hsize_t file_dims[3];         // size H x W x (number of frames)
  int data_element_size;        // 4 for 32 bit ints, 2 for 16
  char *data_buffer;            // Where we'll put our data
  int   data_buffer_size;       // number of bytes to store a frame
  hid_t mem_space;              // where we'll put our data according to h5
  hsize_t mem_dims[2];          // size of our memory accrding to h5
  hsize_t start[3];             // our data slice that includes our frame
  hsize_t stride[3];            // a single step toward our frame
  hsize_t count[3];             // number of frames to select (yeah, it's one)
  hsize_t block[3];             // size of block to select (Spoiler alert: it's one frame)
  isImageBufType *imb;

  imb   = *imbp;
  extra = imb->extra;

  for (fp = extra->frame_discovery_base; fp != NULL; fp = fp->next) {
    if (fp->first_frame <= imb->frame && fp->last_frame >= imb->frame) {
      break;
    }
  }
  if (fp == NULL) {
    isLogging_err("%s: Could not find frame %d in file %s\n", id, imb->frame, imb->key);
    return -1;
  }

  rank = H5Sget_simple_extent_ndims(fp->file_space);
  if (rank < 0) {
    isLogging_err("%s: Failed to get rank of dataset for file %s\n", id, imb->key);
    return -1;
  }

  if (rank != 3) {
    isLogging_err("%s: Unexpected value of data_set rank.  Got %d but should gotten 3\n", id, rank);
    return -1;
  }

  herr = H5Sget_simple_extent_dims( fp->file_space, file_dims, NULL);
  if (herr < 0) {
    isLogging_err("Could not get dataset dimensions\n");
    exit (-1);
  }

  data_element_size = H5Tget_size( fp->file_type);
  if (data_element_size == 0) {
    isLogging_err("%s: Could not get data_element_size\n", id);
    return -1;
  }

  switch(data_element_size) {
  case 4:
    data_buffer_size = file_dims[1] * file_dims[2] * sizeof(uint32_t);
    break;
  case 2:
    data_buffer_size = file_dims[1] * file_dims[2] * sizeof(uint16_t);
    break;

  default:
    isLogging_err("%s: Bad data element size, received %d instead of 2 or 4\n", id, data_element_size);
    return -1;
  }

  data_buffer = calloc(data_buffer_size, 1);
  if (data_buffer == NULL) {
    isLogging_crit("%s: Out of memory (data_buffer)\n", id);
    exit (-1);
  }

  mem_dims[0] = file_dims[1];
  mem_dims[1] = file_dims[2];
  mem_space = H5Screate_simple(2, mem_dims, mem_dims);
  if (mem_space < 0) {
    isLogging_err("%s: Could not create mem_space\n", id);
    free(data_buffer);
    return -1;
  }

  start[0] = imb->frame - fp->first_frame;
  start[1] = 0;
  start[2] = 0;

  stride[0] = 1;
  stride[1] = 1;
  stride[2] = 1;

  count[0] = 1;
  count[1] = 1;
  count[2] = 1;

  block[0] = 1;
  block[1] = file_dims[1];
  block[2] = file_dims[2];

  herr = H5Sselect_hyperslab(fp->file_space, H5S_SELECT_SET, start, stride, count, block);
  if (herr < 0) {
    isLogging_err("%s: Could not set hyperslab for frame %d\n", id, imb->frame);
    return -1;
  }
    
  herr = H5Dread(fp->data_set, fp->file_type, mem_space, fp->file_space, H5P_DEFAULT, data_buffer);
  if (herr < 0) {
    isLogging_err("%s: Could not read frame %d\n", id, imb->frame);
    return -1;
  }

  imb->buf = data_buffer;
  imb->buf_size   = data_buffer_size;
  imb->buf_height = file_dims[1];
  imb->buf_width  = file_dims[2];
  imb->buf_depth  = data_element_size;

  return 0;
}

/** Return a single frame from the named file.
 **
 ** @param[in] fn  name of the file
 **
 ** @param[out] imb frame buffer to place our info in
 **
 ** @returns 0 on success
 */
int isH5GetData(isWorkerContext_t *wctx, const char *fn, isImageBufType **imbp) {
  static const char *id = FILEID "isH5GetData";
  isH5extra_t *extra;           // pointer to frame discovery list
  hid_t master_file;            // the master file, of course
  herr_t herr;                  // h5 error code
  hid_t data_set;               // bad pixel map in h5 file
  hid_t data_space;             // h5 data space for bad pixel map
  hsize_t dims[2];              // dimensions of bad pixel map
  int rank;                     // number of pixel map dimensions (it had better be 2)
  int npoints;                  // number of entries in the bad pixel map
  int err;                      // error code from routines that return integer error codes
  uint32_t first_frame;         // first frame referenced by master file
  uint32_t last_frame;          // last frame referenced by master file
  frame_discovery_t *fp;        // used to loop through discovery list
  int failed;                   // set to 1 before breaking out of the our box
  isImageBufType *imb;
  
  imb = *imbp;

  failed = 0;
  extra = imb->extra;

  pthread_mutex_lock(&wctx->metaMutex);
  set_json_object_integer(id, imb->meta, "frame", imb->frame);
  pthread_mutex_unlock(&wctx->metaMutex);

  //
  // Open up the master file
  //
  master_file = H5Fopen(fn, H5F_ACC_RDONLY, H5P_DEFAULT);
  if (master_file < 0) {
    isLogging_err("%s: Could not open master file %s\n", id, fn);
    return -1;
  }

  if (extra == NULL) {
    extra = calloc(1, sizeof(*(extra)));
    if (extra == NULL) {
      isLogging_err("%s: Out of memory (extra)\n", id);
      exit(-1);
    }
    extra->frame_discovery_base = NULL;

    imb->extra = extra;
    //
    // Find which frame is where
    //
    //isLogging_debug("%s: visiting file %s", id, fn);
    herr = H5Lvisit_by_name(master_file, "/entry/data", H5_INDEX_NAME, H5_ITER_INC, discovery_cb, extra, H5P_DEFAULT);
    if (herr < 0) {
      isLogging_err("%s: Could not discover which frame is where for file %s\n", id, fn);
      return -1;
    }
    
    first_frame = 0xffffffff;
    last_frame  = 0;
    for (fp = extra->frame_discovery_base; fp != NULL; fp = fp->next) {
      first_frame = first_frame < fp->first_frame ? first_frame : fp->first_frame;
      last_frame  = last_frame  > fp->last_frame  ? last_frame  : fp->last_frame;
    }

    pthread_mutex_lock(&wctx->metaMutex);
    set_json_object_integer(id, imb->meta, "first_frame", first_frame);
    set_json_object_integer(id, imb->meta, "last_frame",  last_frame);
    pthread_mutex_unlock(&wctx->metaMutex);
  
    //
    // Our error breakout box
    //
    do {
      //
      // Get the bad pixel map
      //
      //isLogging_debug("%s: calling H5Dopen2 for pixel mask", id);
      data_set = H5Dopen2(master_file, "/entry/instrument/detector/detectorSpecific/pixel_mask", H5P_DEFAULT);
      if (data_set < 0) {
        isLogging_err("%s: Could not open pixel mask data set\n", id);
        failed = 1;
        break;
      }
      
      data_space = H5Dget_space(data_set);
      if (data_space < 0) {
        isLogging_err("%s: Could not open pixel mask data space\n", id);
        failed = 1;
        break;
      }
      
      rank = H5Sget_simple_extent_ndims(data_space);
      if (rank < 0) {
        isLogging_err("%s: Could not get pixel mask rank\n", id);
        failed = 1;
        break;
      }
      
      if (rank != 2) {
        isLogging_err("%s: We do not know how to deal with a pixel mask of rank %d.  It should be 2\n", id, rank);
        failed = 1;
        break;
      }
      
      err = H5Sget_simple_extent_dims(data_space, dims, NULL);
      if (err < 0) {
        isLogging_err("%s: Could not get pixelmask dimensions\n", id);
        failed = 1;
        break;
      }
      
      npoints = H5Sget_simple_extent_npoints(data_space);
      if (npoints < 0) {
        isLogging_err("%s: Could not get pixel mask dimensions\n", id);
        failed = 1;
        break;
      }
      
      imb->bad_pixel_map = calloc(npoints, sizeof(uint32_t));
      if (imb->bad_pixel_map == NULL) {
        isLogging_err("%s: Could not allocate memory for the pixelmask\n", id);
        failed = 1;
        break;
      }
      
      err = H5Dread(data_set, H5T_NATIVE_UINT, H5S_ALL, H5S_ALL, H5P_DEFAULT, imb->bad_pixel_map);
      if (err < 0) {
        isLogging_err("%s: Could not read pixelmask data\n", id);
        free(imb->bad_pixel_map);
        imb->bad_pixel_map = NULL;
        failed = 1;
        break;
      }
    } while(0);
  }

  err = -1;
  if (!failed) {
    err = get_one_frame(imbp);
  }
  
  // These close routines return < 0 on error but we do not have
  // anything we are going to do about it so we will not even check.
  //

  if (data_space >= 0) {
    H5Sclose(data_space);
  }

  if (data_set >= 0) {
    H5Dclose(data_set);
  }

  if (master_file >= 0) {
    H5Fclose(master_file);
  }

  return err;
}
