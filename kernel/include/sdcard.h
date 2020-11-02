#ifndef _SDCARD_H
#define _SDCARD_H

#ifdef __cplusplus
 extern "C" {
#endif

/** 
  * @brief  Card Specific Data: CSD Register   
  */ 
typedef struct {
	uint8  CSDStruct;            /*!< CSD structure */
	uint8  SysSpecVersion;       /*!< System specification version */
	uint8  Reserved1;            /*!< Reserved */
	uint8  TAAC;                 /*!< Data read access-time 1 */
	uint8  NSAC;                 /*!< Data read access-time 2 in CLK cycles */
	uint8  MaxBusClkFrec;        /*!< Max. bus clock frequency */
	uint16 CardComdClasses;      /*!< Card command classes */
	uint8  RdBlockLen;           /*!< Max. read data block length */
	uint8  PartBlockRead;        /*!< Partial blocks for read allowed */
	uint8  WrBlockMisalign;      /*!< Write block misalignment */
	uint8  RdBlockMisalign;      /*!< Read block misalignment */
	uint8  DSRImpl;              /*!< DSR implemented */
	uint8  Reserved2;            /*!< Reserved */
	uint32 DeviceSize;           /*!< Device Size */
	uint8  MaxRdCurrentVDDMin;   /*!< Max. read current @ VDD min */
	uint8  MaxRdCurrentVDDMax;   /*!< Max. read current @ VDD max */
	uint8  MaxWrCurrentVDDMin;   /*!< Max. write current @ VDD min */
	uint8  MaxWrCurrentVDDMax;   /*!< Max. write current @ VDD max */
	uint8  DeviceSizeMul;        /*!< Device size multiplier */
	uint8  EraseGrSize;          /*!< Erase group size */
	uint8  EraseGrMul;           /*!< Erase group size multiplier */
	uint8  WrProtectGrSize;      /*!< Write protect group size */
	uint8  WrProtectGrEnable;    /*!< Write protect group enable */
	uint8  ManDeflECC;           /*!< Manufacturer default ECC */
	uint8  WrSpeedFact;          /*!< Write speed factor */
	uint8  MaxWrBlockLen;        /*!< Max. write data block length */
	uint8  WriteBlockPaPartial;  /*!< Partial blocks for write allowed */
	uint8  Reserved3;            /*!< Reserded */
	uint8  ContentProtectAppli;  /*!< Content protection application */
	uint8  FileFormatGrouop;     /*!< File format group */
	uint8  CopyFlag;             /*!< Copy flag (OTP) */
	uint8  PermWrProtect;        /*!< Permanent write protection */
	uint8  TempWrProtect;        /*!< Temporary write protection */
	uint8  FileFormat;           /*!< File Format */
	uint8  ECC;                  /*!< ECC code */
	uint8  CSD_CRC;              /*!< CSD CRC */
	uint8  Reserved4;            /*!< always 1*/
} SD_CSD;

/** 
  * @brief  Card Identification Data: CID Register   
  */
typedef struct {
	uint8  ManufacturerID;       /*!< ManufacturerID */
	uint16 OEM_AppliID;          /*!< OEM/Application ID */
	uint32 ProdName1;            /*!< Product Name part1 */
	uint8  ProdName2;            /*!< Product Name part2*/
	uint8  ProdRev;              /*!< Product Revision */
	uint32 ProdSN;               /*!< Product Serial Number */
	uint8  Reserved1;            /*!< Reserved1 */
	uint16 ManufactDate;         /*!< Manufacturing Date */
	uint8  CID_CRC;              /*!< CID CRC */
	uint8  Reserved2;            /*!< always 1 */
} SD_CID;

/** 
  * @brief SD Card information 
  */
typedef struct {
	SD_CSD SD_csd;
	SD_CID SD_cid;
	uint64 CardCapacity;  /*!< Card Capacity */
	uint32 CardBlockSize; /*!< Card Block Size */
} SD_CardInfo;

extern SD_CardInfo cardinfo;

uint8 sd_init(void);
void sdcard_init(void);
uint8 sd_read_sector(uint8 *data_buff, uint32 sector, uint32 count);
uint8 sd_write_sector(uint8 *data_buff, uint32 sector, uint32 count);
uint8 sd_read_sector_dma(uint8 *data_buff, uint32 sector, uint32 count);
uint8 sd_write_sector_dma(uint8 *data_buff, uint32 sector, uint32 count);

#ifdef __cplusplus
}
#endif

#endif
