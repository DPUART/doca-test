<!---
/*
 * Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of NVIDIA CORPORATION &
 * AFFILIATES (the "Company") and all right, title, and interest in and to the
 * software product, including all associated intellectual property rights, are
 * and shall remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 *
 */
--->

# 1. Intro
```
The doca_sha_offloading_engine is an OpenSSL dynamic engine with the ability of offloaidng SHA calculation.
The OpenSSL dynamic engine can be referenced in https://www.openssl.org/docs/man1.1.1/man1/openssl-engine.html.
This engine can offload the OpenSSL sha1, sha256, sha512.
This engine is based on the OpenSSL high-level algorithm call interface: EVP_Digest.
It can be referenced in https://www.openssl.org/docs/manmaster/man3/EVP_Digest.html.
This engine supports one-shot SHA calculation.
Its test is based on official version: OpenSSL-1.1.1f on ubuntu 20.04 lts, and OpenSSL-3.0.2 on ubuntu 22.04 lts.
It requires the OpenSSL version >= 1.1.1.
```

# 2. Check the engine can be loaded.
```
$ openssl engine dynamic -pre NO_VCHECK:1 -pre SO_PATH:${DOCA_DIR}/infrastructure/doca_sha_offload_engine/libdoca_sha_offload_engine.so -pre LOAD -vvv -t -c
(dynamic) Dynamic engine loading support
[Success]: SO_PATH:${DOCA_DIR}/infrastructure/doca_sha_offload_engine/libdoca_sha_offload_engine.so
[Success]: LOAD
Loaded: (doca_sha_offload_engine) Openssl SHA offloading engine based on doca_sha
 [SHA1, SHA256, SHA512]
     [ available ]
     set_pci_addr: set the pci address of the doca_sha_engine
          (input flags): STRING
```

# 3. Check the engine can do SHA-1, SHA-256, SHA-512

## SHA-1
```
$ echo "hello world" | openssl dgst -sha1 -engine ${DOCA_DIR}/infrastructure/doca_sha_offload_engine/libdoca_sha_offload_engine.so -engine_impl
engine "doca_sha_offload_engine" set.
(stdin)= 22596363b3de40b06f981fb85d82312e8c0ed511
```

## SHA-256
```
$ echo "hello world" | openssl dgst -sha256 -engine ${DOCA_DIR}/infrastructure/doca_sha_offload_engine/libdoca_sha_offload_engine.so -engine_impl
engine "doca_sha_offload_engine" set.
(stdin)= a948904f2f0f479b8f8197694b30184b0d2ed1c1cd2a1ec0fb85d299a192a447
```

## SHA-512
```
$ echo "hello world" | openssl dgst -sha512 -engine ${DOCA_DIR}/infrastructure/doca_sha_offload_engine/libdoca_sha_offload_engine.so -engine_impl
engine "doca_sha_offload_engine" set.
(stdin)= db3974a97f2407b7cae1ae637c0030687a11913274d578492558e39c16c017de84eacdc8c62fe34ee4e12b4b1428817f09b6a2760c3f8a664ceae94d2434a593
```

#4. 'openssl speed' is the official OpenSSL performance benchmark tool, https://www.openssl.org/docs/man1.1.1/man1/openssl-speed.html. The doca_sha_offload_engine throughput can be measured by 'openssl speed'.

## ssl speed test, sha1, each job 10000 bytes, using engine:
```
$ openssl speed -evp sha1 -bytes 10000 -elapsed --engine ${DOCA_DIR}/infrastructure/doca_sha_offload_engine/libdoca_sha_offload_engine.so
```

## ssl speed test, sha1, each job 10000 bytes, using engine, async_jobs = 256:
```
$ openssl speed -evp sha1 -bytes 10000 -elapsed --engine ${DOCA_DIR}/infrastructure/doca_sha_offload_engine/libdoca_sha_offload_engine.so -async_jobs 256
```

## ssl speed test, sha1, each job 10000 bytes, using engine, async_jobs = 256, threads = 8:
```
$ openssl speed -evp sha1 -bytes 10000 -elapsed --engine ${DOCA_DIR}/infrastructure/doca_sha_offload_engine/libdoca_sha_offload_engine.so -async_jobs 256 -multi 8
```

#5. This engine can be called by an OpenSSL application using standard OpenSSL dynamic engine loading APIs.
Its reference is in https://www.openssl.org/docs/man1.0.2/man3/engine.html.
For example, the load engine can be implemented as:
```
	ENGINE *e;
	const char *doca_engine_path = "${DOCA_DIR}/infrastructure/doca_sha_offload_engine/libdoca_sha_offload_engine.so";
	const char *default_doca_pci_addr = "03:00.0";
	ENGINE_load_dynamic();
	e = ENGINE_by_id(doca_engine_path);
	ENGINE_ctrl_cmd_string(e, "set_pci_addr", doca_engine_pci_addr, 0);
	ENGINE_init(e);
	ENGINE_set_default_digests(e);
```

And the unload engine can be implemented as:
```
	ENGINE_unregister_digests(e);
	ENGINE_finish(e);
	ENGINE_free(e);
```
