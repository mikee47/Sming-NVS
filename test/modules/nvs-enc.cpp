// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <NvsTest.h>
#include <Nvs/PartitionManager.hpp>
#include <Data/Stream/MemoryDataStream.h>
#if CONFIG_NVS_ENCRYPTION
#include <mbedtls/aes.h>
#endif

#define TEST_ESP_ERR(rc, res) CHECK((rc) == (res))
#define TEST_ESP_OK(rc) CHECK((rc) == ESP_OK)

using namespace nvs;

class NvsEncTest : public TestGroup
{
public:
	NvsEncTest() : TestGroup(_F("NVS Encrypted"))
	{
	}

	void execute() override
	{
#if CONFIG_NVS_ENCRYPTION
		TEST_CASE("check underlying xts code for 32-byte size sector encryption")
		{
			auto toHex = [](char ch) {
				if(ch >= '0' && ch <= '9')
					return ch - '0';
				else if(ch >= 'a' && ch <= 'f')
					return ch - 'a' + 10;
				else if(ch >= 'A' && ch <= 'F')
					return ch - 'A' + 10;
				else
					return 0;
			};

			auto toHexByte = [toHex](char* c) { return 16 * toHex(c[0]) + toHex(c[1]); };

			auto toHexStream = [toHexByte](char* src, uint8_t* dest) {
				uint32_t cnt = 0;
				char* p = src;
				while(*p != '\0' && *(p + 1) != '\0') {
					dest[cnt++] = toHexByte(p);
					p += 2;
				}
			};

			uint8_t eky_hex[2 * EncryptionKey.keySize];
			uint8_t ptxt_hex[Page::ENTRY_SIZE], ctxt_hex[Page::ENTRY_SIZE], ba_hex[16];
			mbedtls_aes_xts_context ectx[1];
			mbedtls_aes_xts_context dctx[1];

			char eky[][2 * EncryptionKey.keySize + 1] = {
				"0000000000000000000000000000000000000000000000000000000000000000",
				"1111111111111111111111111111111111111111111111111111111111111111"};
			char tky[][2 * EncryptionKey.keySize + 1] = {
				"0000000000000000000000000000000000000000000000000000000000000000",
				"2222222222222222222222222222222222222222222222222222222222222222"};
			char blk_addr[][2 * 16 + 1] = {"00000000000000000000000000000000", "33333333330000000000000000000000"};

			char ptxt[][2 * Page::ENTRY_SIZE + 1] = {
				"0000000000000000000000000000000000000000000000000000000000000000",
				"4444444444444444444444444444444444444444444444444444444444444444"};
			char ctxt[][2 * Page::ENTRY_SIZE + 1] = {
				"d456b4fc2e620bba6ffbed27b956c9543454dd49ebd8d8ee6f94b65cbe158f73",
				"e622334f184bbce129a25b2ac76b3d92abf98e22df5bdd15af471f3db8946a85"};

			mbedtls_aes_xts_init(ectx);
			mbedtls_aes_xts_init(dctx);

			for(uint8_t cnt = 0; cnt < sizeof(eky) / sizeof(eky[0]); cnt++) {
				toHexStream(eky[cnt], eky_hex);
				toHexStream(tky[cnt], &eky_hex[EncryptionKey.keySize]);
				toHexStream(ptxt[cnt], ptxt_hex);
				toHexStream(ctxt[cnt], ctxt_hex);
				toHexStream(blk_addr[cnt], ba_hex);

				CHECK(!mbedtls_aes_xts_setkey_enc(ectx, eky_hex, 2 * EncryptionKey.keySize * 8));
				CHECK(!mbedtls_aes_xts_setkey_enc(dctx, eky_hex, 2 * EncryptionKey.keySize * 8));

				CHECK(!mbedtls_aes_crypt_xts(ectx, MBEDTLS_AES_ENCRYPT, Page::ENTRY_SIZE, ba_hex, ptxt_hex, ptxt_hex));

				CHECK(!memcmp(ptxt_hex, ctxt_hex, Page::ENTRY_SIZE));
			}
		}

		TEST_CASE("test nvs apis with encryption enabled")
		{
			nvs_handle_t handle_1;
			const uint32_t NVS_FLASH_SECTOR = 6;
			const uint32_t NVS_FLASH_SECTOR_COUNT_MIN = 3;

			nvs_sec_cfg_t xts_cfg;
			for(int count = 0; count < EncryptionKey.keySize; count++) {
				xts_cfg.eky[count] = 0x11;
				xts_cfg.tky[count] = 0x22;
			}
			EncryptedPartitionFixture fixture(&xts_cfg, NVS_FLASH_SECTOR, NVS_FLASH_SECTOR_COUNT_MIN);
			fixture.emu.randomize(100);
			fixture.emu.setBounds(NVS_FLASH_SECTOR, NVS_FLASH_SECTOR + NVS_FLASH_SECTOR_COUNT_MIN);

			for(uint16_t i = NVS_FLASH_SECTOR; i < NVS_FLASH_SECTOR + NVS_FLASH_SECTOR_COUNT_MIN; ++i) {
				fixture.emu.erase(i);
			}
			TEST_ESP_OK(NVSPartitionManager::get_instance()->init_custom(&fixture.part, NVS_FLASH_SECTOR,
																		 NVS_FLASH_SECTOR_COUNT_MIN));

			TEST_ESP_ERR(nvs_open("namespace1", NVS_READONLY, &handle_1), ESP_ERR_NVS_NOT_FOUND);

			TEST_ESP_OK(nvs_open("namespace1", NVS_READWRITE, &handle_1));
			TEST_ESP_OK(nvs_set_i32(handle_1, "foo", 0x12345678));
			TEST_ESP_OK(nvs_set_i32(handle_1, "foo", 0x23456789));

			nvs_handle_t handle_2;
			TEST_ESP_OK(nvs_open("namespace2", NVS_READWRITE, &handle_2));
			TEST_ESP_OK(nvs_set_i32(handle_2, "foo", 0x3456789a));
			const char* str = "value 0123456789abcdef0123456789abcdef";
			TEST_ESP_OK(nvs_set_str(handle_2, "key", str));

			int32_t v1;
			TEST_ESP_OK(nvs_get_i32(handle_1, "foo", &v1));
			CHECK(0x23456789 == v1);

			int32_t v2;
			TEST_ESP_OK(nvs_get_i32(handle_2, "foo", &v2));
			CHECK(0x3456789a == v2);

			char buf[strlen(str) + 1];
			size_t buf_len = sizeof(buf);

			size_t buf_len_needed;
			TEST_ESP_OK(nvs_get_str(handle_2, "key", nullptr, &buf_len_needed));
			CHECK(buf_len_needed == buf_len);

			size_t buf_len_short = buf_len - 1;
			TEST_ESP_ERR(ESP_ERR_NVS_INVALID_LENGTH, nvs_get_str(handle_2, "key", buf, &buf_len_short));
			CHECK(buf_len_short == buf_len);

			size_t buf_len_long = buf_len + 1;
			TEST_ESP_OK(nvs_get_str(handle_2, "key", buf, &buf_len_long));
			CHECK(buf_len_long == buf_len);

			TEST_ESP_OK(nvs_get_str(handle_2, "key", buf, &buf_len));

			CHECK(0 == strcmp(buf, str));
			nvs_close(handle_1);
			nvs_close(handle_2);
			TEST_ESP_OK(nvs_flash_deinit());
		}

		TEST_CASE("test nvs apis for nvs partition generator utility with encryption enabled", "[nvs_part_gen]")
		{
			int status;
			int childpid = fork();
			if(childpid == 0) {
				exit(execlp("cp", " cp", "-rf", "../nvs_partition_generator/testdata", ".", nullptr));
			} else {
				CHECK(childpid > 0);
				waitpid(childpid, &status, 0);
				CHECK(WEXITSTATUS(status) == 0);

				childpid = fork();

				if(childpid == 0) {
					exit(execlp("python", "python", "../nvs_partition_generator/nvs_partition_gen.py", "encrypt",
								"../nvs_partition_generator/sample_multipage_blob.csv", "partition_encrypted.bin",
								"0x4000", "--inputkey",
								"../nvs_partition_generator/testdata/sample_encryption_keys.bin", "--outdir",
								"../nvs_partition_generator", nullptr));
				} else {
					CHECK(childpid > 0);
					waitpid(childpid, &status, 0);
					CHECK(WEXITSTATUS(status) == 0);
				}
			}

			FlashEmulator emu("../nvs_partition_generator/partition_encrypted.bin");

			nvs_sec_cfg_t cfg;
			for(int count = 0; count < EncryptionKey.keySize; count++) {
				cfg.eky[count] = 0x11;
				cfg.tky[count] = 0x22;
			}

			check_nvs_part_gen_args(&emu, NVS_DEFAULT_PART_NAME, 4,
									"../nvs_partition_generator/testdata/sample_multipage_blob.bin", true, &cfg);

			childpid = fork();
			if(childpid == 0) {
				exit(execlp("rm", " rm", "-rf", "testdata", nullptr));
			} else {
				CHECK(childpid > 0);
				waitpid(childpid, &status, 0);
				CHECK(WEXITSTATUS(status) == 0);
			}
		}

		TEST_CASE("test decrypt functionality for encrypted data", "[nvs_part_gen]")
		{
			//retrieving the temporary test data
			int status = system("cp -rf ../nvs_partition_generator/testdata .");
			CHECK(status == 0);

			//encoding data from sample_multipage_blob.csv
			status =
				system("python ../nvs_partition_generator/nvs_partition_gen.py generate "
					   "../nvs_partition_generator/sample_multipage_blob.csv partition_encoded.bin 0x5000 --outdir "
					   "../nvs_partition_generator");
			CHECK(status == 0);

			//encrypting data from sample_multipage_blob.csv
			status = system(
				"python ../nvs_partition_generator/nvs_partition_gen.py encrypt "
				"../nvs_partition_generator/sample_multipage_blob.csv partition_encrypted.bin 0x5000 --inputkey "
				"../nvs_partition_generator/testdata/sample_encryption_keys.bin --outdir ../nvs_partition_generator");
			CHECK(status == 0);

			//decrypting data from partition_encrypted.bin
			status = system("python ../nvs_partition_generator/nvs_partition_gen.py decrypt "
							"../nvs_partition_generator/partition_encrypted.bin "
							"../nvs_partition_generator/testdata/sample_encryption_keys.bin "
							"../nvs_partition_generator/partition_decrypted.bin");
			CHECK(status == 0);

			status = system("diff ../nvs_partition_generator/partition_decrypted.bin "
							"../nvs_partition_generator/partition_encoded.bin");
			CHECK(status == 0);
			CHECK(WEXITSTATUS(status) == 0);

			//cleaning up the temporary test data
			status = system("rm -rf testdata");
			CHECK(status == 0);
		}

		TEST_CASE("test nvs apis for nvs partition generator utility with encryption enabled using keygen",
				  "[nvs_part_gen]")
		{
			int childpid = fork();
			int status;

			if(childpid == 0) {
				exit(execlp("cp", " cp", "-rf", "../nvs_partition_generator/testdata", ".", nullptr));
			} else {
				CHECK(childpid > 0);
				waitpid(childpid, &status, 0);
				CHECK(WEXITSTATUS(status) == 0);

				childpid = fork();

				if(childpid == 0) {
					exit(execlp("rm", " rm", "-rf", "../nvs_partition_generator/keys", nullptr));
				} else {
					CHECK(childpid > 0);
					waitpid(childpid, &status, 0);
					CHECK(WEXITSTATUS(status) == 0);

					childpid = fork();
					if(childpid == 0) {
						exit(execlp("python", "python", "../nvs_partition_generator/nvs_partition_gen.py", "encrypt",
									"../nvs_partition_generator/sample_multipage_blob.csv",
									"partition_encrypted_using_keygen.bin", "0x4000", "--keygen", "--outdir",
									"../nvs_partition_generator", nullptr));

					} else {
						CHECK(childpid > 0);
						waitpid(childpid, &status, 0);
						CHECK(WEXITSTATUS(status) == 0);
					}
				}
			}

			DIR* dir;
			struct dirent* file;
			char* filename;
			char* files;
			char* file_ext;

			dir = opendir("../nvs_partition_generator/keys");
			while((file = readdir(dir)) != nullptr) {
				filename = file->d_name;
				files = strrchr(filename, '.');
				if(files != nullptr) {
					file_ext = files + 1;
					if(strncmp(file_ext, "bin", 3) == 0) {
						break;
					}
				}
			}

			std::string encr_file = std::string("../nvs_partition_generator/keys/") + std::string(filename);
			FlashEmulator emu("../nvs_partition_generator/partition_encrypted_using_keygen.bin");

			char buffer[64];
			FILE* fp;

			fp = fopen(encr_file.c_str(), "rb");
			fread(buffer, sizeof(buffer), 1, fp);

			fclose(fp);

			nvs_sec_cfg_t cfg;

			for(int count = 0; count < EncryptionKey.keySize; count++) {
				cfg.eky[count] = buffer[count] & 255;
				cfg.tky[count] = buffer[count + 32] & 255;
			}

			check_nvs_part_gen_args(&emu, NVS_DEFAULT_PART_NAME, 4,
									"../nvs_partition_generator/testdata/sample_multipage_blob.bin", true, &cfg);
		}

		TEST_CASE("test nvs apis for nvs partition generator utility with encryption enabled using inputkey",
				  "[nvs_part_gen]")
		{
			int childpid = fork();
			int status;

			DIR* dir;
			struct dirent* file;
			char* filename;
			char* files;
			char* file_ext;

			dir = opendir("../nvs_partition_generator/keys");
			while((file = readdir(dir)) != nullptr) {
				filename = file->d_name;
				files = strrchr(filename, '.');
				if(files != nullptr) {
					file_ext = files + 1;
					if(strncmp(file_ext, "bin", 3) == 0) {
						break;
					}
				}
			}

			std::string encr_file = std::string("../nvs_partition_generator/keys/") + std::string(filename);

			if(childpid == 0) {
				exit(execlp("python", "python", "../nvs_partition_generator/nvs_partition_gen.py", "encrypt",
							"../nvs_partition_generator/sample_multipage_blob.csv",
							"partition_encrypted_using_keyfile.bin", "0x4000", "--inputkey", encr_file.c_str(),
							"--outdir", "../nvs_partition_generator", nullptr));

			} else {
				CHECK(childpid > 0);
				waitpid(childpid, &status, 0);
				CHECK(WEXITSTATUS(status) == 0);
			}

			FlashEmulator emu("../nvs_partition_generator/partition_encrypted_using_keyfile.bin");

			char buffer[64];
			FILE* fp;

			fp = fopen(encr_file.c_str(), "rb");
			fread(buffer, sizeof(buffer), 1, fp);

			fclose(fp);

			nvs_sec_cfg_t cfg;

			for(int count = 0; count < EncryptionKey.keySize; count++) {
				cfg.eky[count] = buffer[count] & 255;
				cfg.tky[count] = buffer[count + 32] & 255;
			}

			check_nvs_part_gen_args(&emu, NVS_DEFAULT_PART_NAME, 4,
									"../nvs_partition_generator/testdata/sample_multipage_blob.bin", true, &cfg);

			childpid = fork();
			if(childpid == 0) {
				exit(execlp("rm", " rm", "-rf", "../nvs_partition_generator/keys", nullptr));
			} else {
				CHECK(childpid > 0);
				waitpid(childpid, &status, 0);
				CHECK(WEXITSTATUS(status) == 0);

				childpid = fork();

				if(childpid == 0) {
					exit(execlp("rm", " rm", "-rf", "testdata", nullptr));
				} else {
					CHECK(childpid > 0);
					waitpid(childpid, &status, 0);
					CHECK(WEXITSTATUS(status) == 0);
				}
			}
		}

		TEST_CASE("check and read data from partition generated via manufacturing utility with encryption enabled "
				  "using sample "
				  "inputkey",
				  "[mfg_gen]")
		{
			int childpid = fork();
			int status;

			if(childpid == 0) {
				exit(execlp("bash", " bash", "-c", "rm -rf ../../../tools/mass_mfg/host_test | \
                    cp -rf ../../../tools/mass_mfg/testdata mfg_testdata | \
                    cp -rf ../nvs_partition_generator/testdata . | \
                    mkdir -p ../../../tools/mass_mfg/host_test",
							nullptr));
			} else {
				CHECK(childpid > 0);
				waitpid(childpid, &status, 0);
				CHECK(WEXITSTATUS(status) == 0);

				childpid = fork();
				if(childpid == 0) {
					exit(execlp("python", "python", "../../../tools/mass_mfg/mfg_gen.py", "generate",
								"../../../tools/mass_mfg/samples/sample_config.csv",
								"../../../tools/mass_mfg/samples/sample_values_multipage_blob.csv", "Test", "0x4000",
								"--outdir", "../../../tools/mass_mfg/host_test", "--version", "2", "--inputkey",
								"mfg_testdata/sample_encryption_keys.bin", nullptr));

				} else {
					CHECK(childpid > 0);
					waitpid(childpid, &status, 0);
					CHECK(WEXITSTATUS(status) == 0);

					childpid = fork();
					if(childpid == 0) {
						exit(execlp("python", "python", "../nvs_partition_generator/nvs_partition_gen.py", "encrypt",
									"../../../tools/mass_mfg/host_test/csv/Test-1.csv",
									"../nvs_partition_generator/Test-1-partition-encrypted.bin", "0x4000", "--version",
									"2", "--inputkey", "testdata/sample_encryption_keys.bin", nullptr));

					} else {
						CHECK(childpid > 0);
						waitpid(childpid, &status, 0);
						CHECK(WEXITSTATUS(status) == 0);
					}
				}
			}

			FlashEmulator emu1("../../../tools/mass_mfg/host_test/bin/Test-1.bin");

			nvs_sec_cfg_t cfg;
			for(int count = 0; count < EncryptionKey.keySize; count++) {
				cfg.eky[count] = 0x11;
				cfg.tky[count] = 0x22;
			}

			check_nvs_part_gen_args(&emu1, NVS_DEFAULT_PART_NAME, 4, "mfg_testdata/sample_multipage_blob.bin", true,
									&cfg);

			FlashEmulator emu2("../nvs_partition_generator/Test-1-partition-encrypted.bin");

			check_nvs_part_gen_args(&emu2, NVS_DEFAULT_PART_NAME, 4, "testdata/sample_multipage_blob.bin", true, &cfg);

			childpid = fork();
			if(childpid == 0) {
				exit(execlp("bash", " bash", "-c", "rm -rf ../../../tools/mass_mfg/host_test | \
                    rm -rf mfg_testdata | \
                    rm -rf testdata",
							nullptr));
			} else {
				CHECK(childpid > 0);
				waitpid(childpid, &status, 0);
				CHECK(WEXITSTATUS(status) == 0);
			}
		}

		TEST_CASE(
			"check and read data from partition generated via manufacturing utility with encryption enabled using new "
			"generated key",
			"[mfg_gen]")
		{
			int childpid = fork();
			int status;

			if(childpid == 0) {
				exit(execlp("bash", " bash", "-c", "rm -rf ../../../tools/mass_mfg/host_test | \
                    cp -rf ../../../tools/mass_mfg/testdata mfg_testdata | \
                    cp -rf ../nvs_partition_generator/testdata . | \
                    mkdir -p ../../../tools/mass_mfg/host_test",
							nullptr));
			} else {
				CHECK(childpid > 0);
				waitpid(childpid, &status, 0);
				CHECK(WEXITSTATUS(status) == 0);

				childpid = fork();
				if(childpid == 0) {
					exit(execlp("python", "python", "../../../tools/mass_mfg/mfg_gen.py", "generate-key", "--outdir",
								"../../../tools/mass_mfg/host_test", "--keyfile", "encr_keys_host_test.bin", nullptr));

				} else {
					CHECK(childpid > 0);
					waitpid(childpid, &status, 0);
					CHECK(WEXITSTATUS(status) == 0);

					childpid = fork();
					if(childpid == 0) {
						exit(execlp("python", "python", "../../../tools/mass_mfg/mfg_gen.py", "generate",
									"../../../tools/mass_mfg/samples/sample_config.csv",
									"../../../tools/mass_mfg/samples/sample_values_multipage_blob.csv", "Test",
									"0x4000", "--outdir", "../../../tools/mass_mfg/host_test", "--version", "2",
									"--inputkey", "../../../tools/mass_mfg/host_test/keys/encr_keys_host_test.bin",
									nullptr));

					} else {
						CHECK(childpid > 0);
						waitpid(childpid, &status, 0);
						CHECK(WEXITSTATUS(status) == 0);

						childpid = fork();
						if(childpid == 0) {
							exit(execlp("python", "python", "../nvs_partition_generator/nvs_partition_gen.py",
										"encrypt", "../../../tools/mass_mfg/host_test/csv/Test-1.csv",
										"../nvs_partition_generator/Test-1-partition-encrypted.bin", "0x4000",
										"--version", "2", "--inputkey",
										"../../../tools/mass_mfg/host_test/keys/encr_keys_host_test.bin", nullptr));

						} else {
							CHECK(childpid > 0);
							waitpid(childpid, &status, 0);
							CHECK(WEXITSTATUS(status) == 0);
						}
					}
				}
			}

			FlashEmulator emu1("../../../tools/mass_mfg/host_test/bin/Test-1.bin");

			char buffer[64];
			FILE* fp;

			fp = fopen("../../../tools/mass_mfg/host_test/keys/encr_keys_host_test.bin", "rb");
			fread(buffer, sizeof(buffer), 1, fp);

			fclose(fp);

			nvs_sec_cfg_t cfg;

			for(int count = 0; count < EncryptionKey.keySize; count++) {
				cfg.eky[count] = buffer[count] & 255;
				cfg.tky[count] = buffer[count + 32] & 255;
			}

			check_nvs_part_gen_args(&emu1, NVS_DEFAULT_PART_NAME, 4, "mfg_testdata/sample_multipage_blob.bin", true,
									&cfg);

			FlashEmulator emu2("../nvs_partition_generator/Test-1-partition-encrypted.bin");

			check_nvs_part_gen_args(&emu2, NVS_DEFAULT_PART_NAME, 4, "testdata/sample_multipage_blob.bin", true, &cfg);

			childpid = fork();
			if(childpid == 0) {
				exit(execlp("bash", " bash", "-c", "rm -rf keys | \
                    rm -rf mfg_testdata | \
                    rm -rf testdata | \
                    rm -rf ../../../tools/mass_mfg/host_test",
							nullptr));
			} else {
				CHECK(childpid > 0);
				waitpid(childpid, &status, 0);
				CHECK(WEXITSTATUS(status) == 0);
			}
		}
#endif

		// Add new tests above
		// This test has to be the final one

		TEST_CASE("dump all performance data")
		{
			Serial.println(_F("===================="));
			Serial.println(_F("Dumping benchmarks"));
			Serial.copyFrom(&s_perf);
			Serial.println(_F("===================="));
		}
	}

private:
	MemoryDataStream s_perf;
};

void REGISTER_TEST(nvs_enc)
{
	registerGroup<NvsEncTest>();
}
