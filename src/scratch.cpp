void print_device(libusb_device* dev) {
  libusb_device_descriptor desc;
  libusb_get_device_descriptor(dev, &desc);

  printf("Device              %p\n", dev);
	printf("bLength             %d\n", desc.bLength            );
	printf("bDescriptorType     %d\n", desc.bDescriptorType    );
	printf("bcdUSB              %d\n", desc.bcdUSB             );
	printf("bDeviceClass        %d\n", desc.bDeviceClass       );
	printf("bDeviceSubClass     %d\n", desc.bDeviceSubClass    );
	printf("bDeviceProtocol     %d\n", desc.bDeviceProtocol    );
	printf("bMaxPacketSize0     %d\n", desc.bMaxPacketSize0    );
	printf("idVendor            %d\n", desc.idVendor           );
	printf("idProduct           %d\n", desc.idProduct          );
	printf("bcdDevice           %d\n", desc.bcdDevice          );
	printf("iManufacturer       %d\n", desc.iManufacturer      );
	printf("iProduct            %d\n", desc.iProduct           );
	printf("iSerialNumber       %d\n", desc.iSerialNumber      );
	printf("bNumConfigurations  %d\n", desc.bNumConfigurations );
}

void print_endpoint(const libusb_endpoint_descriptor* pendpoint, int index) {

  printf("Endpoint            %d @ %p\n", index, pendpoint);
	printf("bLength             %d\n", pendpoint->bLength);
	printf("bDescriptorType     %d\n", pendpoint->bDescriptorType);
	printf("bEndpointAddress    %d\n", pendpoint->bEndpointAddress);
	printf("bmAttributes        %d\n", pendpoint->bmAttributes);
	printf("wMaxPacketSize      %d\n", pendpoint->wMaxPacketSize);
	printf("bInterval           %d\n", pendpoint->bInterval);
	printf("bRefresh            %d\n", pendpoint->bRefresh);
	printf("bSynchAddress       %d\n", pendpoint->bSynchAddress);
}

void print_interface(const libusb_interface* pinterface, int index) {
  printf("Interface           %d @ %p\n", index, pinterface);
  for (int i = 0; i < pinterface->num_altsetting; i++) {
    auto alt = &pinterface->altsetting[i];
    printf("Alt                 %d @ %p\n", i, alt);
    printf("bLength             %d\n", alt->bLength);
    printf("bDescriptorType     %d\n", alt->bDescriptorType );
    printf("bInterfaceNumber    %d\n", alt->bInterfaceNumber );
    printf("bAlternateSetting   %d\n", alt->bAlternateSetting );
    printf("bNumEndpoints       %d\n", alt->bNumEndpoints );
    printf("bInterfaceClass     %d\n", alt->bInterfaceClass );
    printf("bInterfaceSubClass  %d\n", alt->bInterfaceSubClass );
    printf("bInterfaceProtocol  %d\n", alt->bInterfaceProtocol );
    printf("iInterface          %d\n", alt->iInterface );

    for (int j = 0; j < alt->bNumEndpoints; j++) {
      auto endpoint = &alt->endpoint[j];
      print_endpoint(endpoint, j);
    }
  }
}

void print_config(libusb_config_descriptor* pconfig, int index) {
  printf("Config descriptor   %d @ %p\n", index, pconfig);
	printf("bLength             %d\n", pconfig->bLength);
	printf("bDescriptorType     %d\n", pconfig->bDescriptorType);
	printf("wTotalLength        %d\n", pconfig->wTotalLength);
	printf("bNumInterfaces      %d\n", pconfig->bNumInterfaces);
	printf("bConfigurationValue %d\n", pconfig->bConfigurationValue);
	printf("iConfiguration      %d\n", pconfig->iConfiguration);
	printf("bmAttributes        %d\n", pconfig->bmAttributes);
	printf("MaxPower            %d\n", pconfig->MaxPower);
  for (int i = 0; i < pconfig->bNumInterfaces; i++) {
    print_interface(&pconfig->interface[i], i);
  }
}
