// Copyright (c) 2015 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENMVG_FEATURES_AKAZE_IMAGE_DESCRIBER_HPP
#define OPENMVG_FEATURES_AKAZE_IMAGE_DESCRIBER_HPP

#include <iostream>
#include <numeric>

#include "openMVG/features/image_describer.hpp"
#include "openMVG/features/regions_factory.hpp"
#include "openMVG/features/akaze/AKAZE.hpp"
#include "openMVG/features/akaze/msurf_descriptor.hpp"
#include "openMVG/features/akaze/mldb_descriptor.hpp"
#include "openMVG/features/liop/liop_descriptor.hpp"

#ifndef OPENMVG_NO_SERIALIZATION
#include <cereal/cereal.hpp>
#endif

using namespace std;

namespace openMVG {
namespace features {

enum EAKAZE_DESCRIPTOR
{
  AKAZE_MSURF,
  AKAZE_LIOP,
  AKAZE_MLDB
};

struct AKAZEParams
{
  AKAZEParams(
    AKAZEConfig config = AKAZEConfig(),
    EAKAZE_DESCRIPTOR eAkazeDescriptor = AKAZE_MSURF
  ):_options(config),_eAkazeDescriptor(eAkazeDescriptor){}

  template<class Archive>
  void serialize(Archive & ar)
  {
    ar(_options, _eAkazeDescriptor);
  }

  // Parameters
  AKAZEConfig _options;
  EAKAZE_DESCRIPTOR _eAkazeDescriptor;
};

class AKAZE_Image_describer : public Image_describer
{
public:
  AKAZE_Image_describer(
    const AKAZEParams & params = AKAZEParams(),
    bool bOrientation = true
  ):Image_describer(), _params(params), _bOrientation(bOrientation) {}


  bool Set_configuration_preset(EDESCRIBER_PRESET preset)
  {
    switch(preset)
    {
    case NORMAL_PRESET:
      _params._options.fThreshold = AKAZEConfig().fThreshold;
    break;
    case HIGH_PRESET:
      _params._options.fThreshold = AKAZEConfig().fThreshold/10.;
    break;
    case ULTRA_PRESET:
     _params._options.fThreshold = AKAZEConfig().fThreshold/100.;
    break;
    default:
      return false;
    }
    return true;
  }

  /**
  @brief Detect regions on the image and compute their attributes (description)
  @param image Image.
  @param regions The detected regions and attributes (the caller must delete the allocated data)
  @param mask 8-bit gray image for keypoint filtering (optional).
     Non-zero values depict the region of interest.
  */
  bool Describe(const image::Image<unsigned char>& image,
    OPENMVG_UNIQUE_PTR<Regions> &regions,
    const image::Image<unsigned char> * mask = NULL)
  {
    _params._options.fDesc_factor =
      (_params._eAkazeDescriptor == AKAZE_MSURF ||
      _params._eAkazeDescriptor == AKAZE_LIOP) ? 10.f*sqrtf(2.f)
      : 11.f*sqrtf(2.f); // MLDB

    AKAZE akaze(image, _params._options);
    akaze.Compute_AKAZEScaleSpace();
    std::vector<AKAZEKeypoint> kpts;
    kpts.reserve(5000);
    akaze.Feature_Detection(kpts);
    akaze.Do_Subpixel_Refinement(kpts);

    Allocate(regions);

    switch(_params._eAkazeDescriptor)
    {
      case AKAZE_MSURF:
      {
        // Build alias to cached data
        AKAZE_Float_Regions * regionsCasted = dynamic_cast<AKAZE_Float_Regions*>(regions.get());
        regionsCasted->Features().resize(kpts.size());
        regionsCasted->Descriptors().resize(kpts.size());

      #ifdef OPENMVG_USE_OPENMP
        #pragma omp parallel for
      #endif
        for (int i = 0; i < static_cast<int>(kpts.size()); ++i)
        {
          AKAZEKeypoint ptAkaze = kpts[i];

          // Feature masking
          if (mask)
          {
            const image::Image<unsigned char> & maskIma = *mask;
            if (maskIma(ptAkaze.y, ptAkaze.x) > 0)
              continue;
          }

          const TEvolution & cur_slice = akaze.getSlices()[ptAkaze.class_id];

          if (_bOrientation)
            akaze.Compute_Main_Orientation(ptAkaze, cur_slice.Lx, cur_slice.Ly);
          else
            ptAkaze.angle = 0.0f;

          regionsCasted->Features()[i] =
            SIOPointFeature(ptAkaze.x, ptAkaze.y, ptAkaze.size, ptAkaze.angle);

          ComputeMSURFDescriptor(cur_slice.Lx, cur_slice.Ly, ptAkaze.octave,
            regionsCasted->Features()[i],
            regionsCasted->Descriptors()[i]);
        }
      }
      break;
      case AKAZE_LIOP:
      {
        // Build alias to cached data
        AKAZE_Liop_Regions * regionsCasted = dynamic_cast<AKAZE_Liop_Regions*>(regions.get());
        regionsCasted->Features().resize(kpts.size());
        regionsCasted->Descriptors().resize(kpts.size());

        // Init LIOP extractor
        LIOP::Liop_Descriptor_Extractor liop_extractor;

      #ifdef OPENMVG_USE_OPENMP
        #pragma omp parallel for
      #endif
        for (int i = 0; i < static_cast<int>(kpts.size()); ++i)
        {
          AKAZEKeypoint ptAkaze = kpts[i];

          // Feature masking
          if (mask)
          {
            const image::Image<unsigned char> & maskIma = *mask;
            if (maskIma(ptAkaze.y, ptAkaze.x) > 0)
              continue;
          }

          const TEvolution & cur_slice = akaze.getSlices()[ptAkaze.class_id];

          if (_bOrientation)
            akaze.Compute_Main_Orientation(ptAkaze, cur_slice.Lx, cur_slice.Ly);
          else
            ptAkaze.angle = 0.0f;

          regionsCasted->Features()[i] =
            SIOPointFeature(ptAkaze.x, ptAkaze.y, ptAkaze.size, ptAkaze.angle);

          // Compute LIOP descriptor (do not need rotation computation, since
          //  LIOP descriptor is rotation invariant).
          // Rescale for LIOP patch extraction
          const SIOPointFeature fp =
              SIOPointFeature(ptAkaze.x, ptAkaze.y,
              ptAkaze.size/2.0, ptAkaze.angle);

          float desc[144];
          liop_extractor.extract(image, fp, desc);
          for(int j=0; j < 144; ++j)
            regionsCasted->Descriptors()[i][j] =
              static_cast<unsigned char>(desc[j] * 255.f +.5f);
        }
      }
      break;
      case AKAZE_MLDB:
      {
        // Build alias to cached data
        AKAZE_Binary_Regions * regionsCasted = dynamic_cast<AKAZE_Binary_Regions*>(regions.get());
        regionsCasted->Features().resize(kpts.size());
        regionsCasted->Descriptors().resize(kpts.size());

      #ifdef OPENMVG_USE_OPENMP
        #pragma omp parallel for
      #endif
        for (int i = 0; i < static_cast<int>(kpts.size()); ++i)
        {
          AKAZEKeypoint ptAkaze = kpts[i];

          // Feature masking
          if (mask)
          {
            const image::Image<unsigned char> & maskIma = *mask;
            if (maskIma(ptAkaze.y, ptAkaze.x) > 0)
              continue;
          }

          const TEvolution & cur_slice = akaze.getSlices()[ptAkaze.class_id];

          if (_bOrientation)
            akaze.Compute_Main_Orientation(ptAkaze, cur_slice.Lx, cur_slice.Ly);
          else
            ptAkaze.angle = 0.0f;

          regionsCasted->Features()[i] =
            SIOPointFeature(ptAkaze.x, ptAkaze.y, ptAkaze.size, ptAkaze.angle);

          // Compute MLDB descriptor
		      Descriptor<bool,486> desc;
      		ComputeMLDBDescriptor(cur_slice.cur, cur_slice.Lx, cur_slice.Ly,
            ptAkaze.octave, regionsCasted->Features()[i], desc);
          // convert the bool vector to the binary unsigned char array
          unsigned char * ptr = reinterpret_cast<unsigned char*>(&regionsCasted->Descriptors()[i]);
          memset(ptr, 0, regionsCasted->DescriptorLength()*sizeof(unsigned char));
          // For each byte
          for (int j = 0; j < std::ceil(486./8.); ++j, ++ptr) {
            // set the corresponding 8bits to the good values
            for (int iBit = 0; iBit < 8 && j*8+iBit < 486; ++iBit)  {
              *ptr |= desc[j*8+iBit] << iBit;
            }
          }
        }
      }
      break;
    }
    return true;
  };

  /// Allocate Regions type depending of the Image_describer
  void Allocate(OPENMVG_UNIQUE_PTR<Regions> &regions) const
  {
    switch(_params._eAkazeDescriptor)
    {
      case AKAZE_MSURF:
        return regions.reset(new AKAZE_Float_Regions);
      break;
      case AKAZE_LIOP:
        return regions.reset(new AKAZE_Liop_Regions);
      case AKAZE_MLDB:
       return regions.reset(new AKAZE_Binary_Regions);
      break;
    }
  }
#ifndef OPENMVG_NO_SERIALIZATION
  template<class Archive>
  void serialize(Archive & ar)
  {
    ar(
     cereal::make_nvp("params", _params),
     cereal::make_nvp("bOrientation", _bOrientation));
  }
#endif // #ifndef OPENMVG_NO_SERIALIZATION


private:
  AKAZEParams _params;
  bool _bOrientation;
};

} // namespace features
} // namespace openMVG

#ifndef OPENMVG_NO_SERIALIZATION
#include <cereal/types/polymorphic.hpp>
#include <cereal/archives/json.hpp>
CEREAL_REGISTER_TYPE_WITH_NAME(openMVG::features::AKAZE_Image_describer, "AKAZE_Image_describer");
#endif // #ifndef OPENMVG_NO_SERIALIZATION

#endif // OPENMVG_FEATURES_AKAZE_IMAGE_DESCRIBER_HPP
