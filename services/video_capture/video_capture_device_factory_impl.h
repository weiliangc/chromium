// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_FACTORY_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_FACTORY_IMPL_H_

#include <vector>

#include "mojo/public/cpp/bindings/binding.h"
#include "services/video_capture/public/interfaces/mock_video_capture_device.mojom.h"
#include "services/video_capture/public/interfaces/video_capture_device_factory.mojom.h"
#include "services/video_capture/video_capture_device_proxy_impl.h"

namespace video_capture {

class VideoCaptureDeviceFactoryImpl : public mojom::VideoCaptureDeviceFactory {
 public:
  VideoCaptureDeviceFactoryImpl(const media::VideoCaptureJpegDecoderFactoryCB&
                                    jpeg_decoder_factory_callback);
  ~VideoCaptureDeviceFactoryImpl() override;

  void AddMojoDevice(std::unique_ptr<VideoCaptureDeviceProxyImpl> device,
                     const media::VideoCaptureDeviceDescriptor& descriptor);

  void AddMediaDevice(std::unique_ptr<media::VideoCaptureDevice> device,
                      const media::VideoCaptureDeviceDescriptor& descriptor);

  void AddMockDevice(mojom::MockVideoCaptureDevicePtr device,
                     const media::VideoCaptureDeviceDescriptor& descriptor);

  // mojom::VideoCaptureDeviceFactory:
  void EnumerateDeviceDescriptors(
      const EnumerateDeviceDescriptorsCallback& callback) override;
  void GetSupportedFormats(
      const media::VideoCaptureDeviceDescriptor& device_descriptor,
      const GetSupportedFormatsCallback& callback) override;
  void CreateDeviceProxy(
      const media::VideoCaptureDeviceDescriptor& device_descriptor,
      mojom::VideoCaptureDeviceProxyRequest proxy_request,
      const CreateDeviceProxyCallback& callback) override;

 private:
  // We use a std::vector of structs with the |descriptor| field as a unique
  // keys. We do it this way instead of using a std::map because Mojo-generated
  // structs (here VideoCaptureDeviceDescriptor) do not (yet) generate a
  // comparison operator, and we do not want to provide a custom one. The
  // performance penalty of linear-time lookup should be minimal assuming that
  // the number of capture devices is typically small.
  class DeviceEntry {
   public:
    DeviceEntry(const media::VideoCaptureDeviceDescriptor& descriptor,
                std::unique_ptr<VideoCaptureDeviceProxyImpl> bindable_target);
    ~DeviceEntry();
    DeviceEntry(DeviceEntry&& other);
    DeviceEntry& operator=(DeviceEntry&& other);

    const media::VideoCaptureDeviceDescriptor& descriptor() const;
    bool DescriptorEquals(
        const media::VideoCaptureDeviceDescriptor& other) const;
    bool is_bound() const;
    void Bind(mojom::VideoCaptureDeviceProxyRequest request);
    void Unbind();

    void OnConnectionErrorOrClose();

   private:
    media::VideoCaptureDeviceDescriptor descriptor_;
    std::unique_ptr<VideoCaptureDeviceProxyImpl> device_proxy_;
    // TODO(chfremer) Use mojo::Binding<> directly instead of unique_ptr<> when
    // mojo::Binding<> supports move operators.
    // https://crbug.com/644314
    std::unique_ptr<mojo::Binding<mojom::VideoCaptureDeviceProxy>> binding_;

    DISALLOW_COPY_AND_ASSIGN(DeviceEntry);
  };

  std::vector<DeviceEntry> devices_;
  media::VideoCaptureJpegDecoderFactoryCB jpeg_decoder_factory_callback_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_FACTORY_IMPL_H_
