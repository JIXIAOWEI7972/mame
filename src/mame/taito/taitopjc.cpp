// license:BSD-3-Clause
// copyright-holders:Philip Bennett
/*
   Taito Power-JC System

   Preliminary driver

   Hardware appears sufficiently different to JC system to warrant
   a separate driver.

   TODO:
   - Inconsistent frame rate compared to PCB recordings; game has a tendency to freeze frame hiccup
   - Analog inputs update at a slower rate (about every 8-10 frames instead of every frame); this issue
     is also present in taitotz.cpp
   - Convert 2d videochip to actual device for the device_memory_interface implications and because it's a
     16-bit i/f (TCG010PJC?)
   - Hook up remaining sound / Zoom hardware - see /src/sony/taito_zm.cpp for reference

   SOUND / ZFX-2 NOTES (state as of this cleanup):
   - The sound board is an MN1020819DA driving a ZSG-2 PCM chip and a Zoom ZFX-2
     (a TMS57002-class DASP).  The MN10200, the ZSG-2 and the MB87078 volume
     controller are emulated; the ZSG-2's four outputs (reverb send, chorus send,
     left/right direct) are mixed straight to the speaker, which is what TeknoTPJC -
     itself built on MAME's devices - does as well (its notice lists only the
     MN10200, ZSG-2 and MB87078).
   - The ZFX-2 is NOT emulated and the tms57002 device is deliberately not
     instantiated: it would only run unprogrammed and produce silence.  The game
     never uploads a DSP program either.  The only host traffic is a boot-time
     bit-serial transfer (0xffc4 bit1 = clock, 0xffc6 bit1 = data) carrying two
     parameter/coefficient blocks taken from tables at 0x08752e (12 + 481*4 bytes)
     and 0x087cc0 (141*3 bytes), and the sound program never touches the DSP host
     port at 0xc00000 afterwards.  That implies the ZFX-2 runs firmware in internal
     (mask) ROM, which MAME's tms57002 device does not model, so its reverb/chorus
     cannot be reproduced without a dump of that firmware.  Experiments with the ZN
     board's uploaded firmware (255 program words + 121 coefficients, captured from
     raystorm/gdarius2, which are byte-identical) run on the device but diverge in
     the DSP program and trip unsupported-opcode paths, so that approach was
     dropped along with the device instantiation.
   - TODO: obtain a ZFX-2 firmware dump (decapped), re-add the tms57002 device, load
     that firmware and feed it the boot-time parameter blocks; then route its
     outputs to the speaker instead of the ZSG-2 mix.  The bit-serial framing is
     documented above so the upload can be decoded once the firmware is available.
   - The MN10200 sound CPU's tick rate was calibrated empirically: the music tempo
     is driven by the interrupt counter, and the ZN boards run theirs at ~1000 Hz.
     mn1020819_device::set_prescaler_percent(550) makes this board match (~1 kHz).
     TODO: replace that fudge with a proper model of the MN1020819DA timer bank
     once the real divider semantics are known.


PCB layout by Brian Troha, thanks to Luke Morse for photos & verification of parts.

Operation Tiger, (c)1998
POWER JC MOTHER-G PCB     K11X0870A  OPERATION TIGER
+----------------------------------------------------------------------------------------------------+
|  |-----------M Connector---------|    |-----------X Connector---------|                            |
|                                     +---------+                                                    |
|  QS32X245 x 3 ABT16245 x 3 E63-05   |D481850GF|   D4516161  D4516161                               |
|                                     +---------+                                                    |
|                                                                                                    |
|        +----------+                 +----------+                                                   |
|  D3N03 |          |                 |          |             IS61LV256                             |
|        | PowerPC  |                 | Taito    |   +------+                                        |
| MAX767 |  603E    |                 | TCG010PJC|   |QS5993|  IS61LV256                             |
|        | QFP240   |                 |  QFP240  |   +------+                 +--------+             |
|        +----------+                 +----------+             IS61LV256      |CXD1178Q|   AD813     |
|                                                                             +--------+             |
|                                           16.5000MHz                                               |
|                                                                                                    |
|  40.0000MHz                  TMS418160    +----------+                                            +-+
|                                           |          |        D4624456    +----------+            |C|
|  +----------+  +----------+  TMS418160    | Taito    |                    |          |            |o|
|  |          |  |          |               | TCO780PFA|        D4624456    |   Heat   |            |n|
|  | TMS320C53|  | IDT7024  |   E63-06      |  QFP240  |                    |   Sink   |            |n| Connects to
|  |      PQ80|  |  QFP84   |     QS32X245  +----------+                    |   for    |            |e| filter board
|  |    QFP116|  |          |   E63-07                                      |   AMP    |            |c|
|  +----------+  +----------+               +----------+        D4624456    |          |            |t|
|                                           |          |                    +----------+            |o|
|                              TMS418160    | Taito    |        D4624456                            |r|
|       IS61C256  E63-01                    | TCO780PFA|                    +----------+            +-+
|                              TMS418160    |  QFP240  |                    |          |             |
|                                           +----------+                    |   Heat   |             |
|       IS61C256  E63-02  ABT16245                                M514256   |   Sink   |             |
|                                          +---------+                      |   for    |             |
|       ABT16245   E63-03 E63-04           |NM1020819|         +-------+    |   AMP    |             |
|                                          +---------+         | ZFX-2 |    |          |             |
|  IDT7201 IDT7201              E63-08                         +-------+    +----------+             |
|                                                                                                    |
|                                   IS61C256                   +-------+    25.0000MHz               |
|                                                              | ZSG-2 |                             |
|                       ABT16245    IS61C256    ABT16245 x 2   +-------+    MB87078  6379   NJM2100  |
|                                                                                                    |
|  |-----------J Connector---------|    |-----------K Connector---------|                            |
+----------------------------------------------------------------------------------------------------+

OSC1 40.0000MHz
OSC2 16.5000MHz
OSC3 25.0000MHz

Main CPU:
   PowerPC-603E QFP240 (driven by IDT QS5993 Clock Driver @ 100MHz?)

Video:
   TMS320BC53PQ80 QFP116 (rated for 80MHz, driven by 40.0000MHz OSC or 40.0000MHz x 2 by IDT QS5993 Clock Driver?)
   Taito TCG010PJC QFP240
   Taito TCO780PFG QFP240 Polygon Renderer
   Sony CXD1178Q QFP48 8-bit High-Speed D/A converter for video band (RGB 3_channel input/output)

Sound:
   Panasonic MN1020819 QFP64 16-Bit Single Chip Microcomputer with 64K x 8 ROM, 3K x 8 RAM (Operational speed of 10MHz-20MHz, driven by 25.0000MHz OSC / 2)
   ZOOM ZFX-2 QFP100 DSP (Functionally identical to TMS57002, driven by 25.0000MHz OSC)
   ZOOM ZSG-2 QFP100 Sound PCM chip (driven by 25.0000MHz OSC)
   uDP6379 2-Channel 16-Bit D/A Audio Converter
   MB87078 6-Bit / 4-Channel Electric Volume Controller
   NJM2100 Dual Operational AMP

ROMs:
   E63-03 & E63-04 are AT27C512 EPROMs and are TMS320BC53PQ80 program ROMs

RAM:
   IDT7024 QFP84 4K x 16 Dual-Port SRAM
   IDT7201 PLCC32 512 x 9 First-In/First-Out Dual-Port Memory
   NEC D4516161AG5 1M x 8 x 2 16M-bit Synchronous DRAM (silkscreened MB81171622)
   NEC D481850GF-A12 QFP100 2 x 128K x 16 32-bit Synchronous Graphics RAM (silscreened NEC MB81G83222)
   IS61LV256-15J 32K x 8 Low Voltage CMOS SRAM  (silkscreened IS61LV256AH)
   TMS418160ADZ-60 1Mb x 16-Bit DRAM (Character Sprite RAM)
   IS61C256AH-15J 32K x 8 High-Speed CMOS SRAM
   NEC uDP482445GW-70 128K x 8 CMOS SRAM
   OKI M514256C-70J 256K x 4 Fast Page DRAM (silkscreened MSM514256)

Misc:
   IDT QS5993 PLCC32 PLL Clock Driver Turboclock, 6.25MHz-100MHz (silkscreened CY7B991)
   D3N03 Power MOSFET 3 Amps, 30 Volts
   Maxim MAX767CAP 5V to 3.3V Synchronous Step-Down Power-Supply Controller
   QS32X245 High-Speed CMOS QuickSwitch Double Width Bus Switch
   ABT16245 16-Bit Bus Transceiver with TriState Outputs
   J,K, M & X 3x32 pin connector connect to POWER JC DAUGHTER PCB (Male with KEY blocks)

PALs:
   E63-01 CE16C8H PAL
   E63-02 CE22C10H PAL
   E63-05 CE16C8H PAL
   E63-06 CE16C8H PAL
   E63-07 CE16C8H PAL
   E63-08 CE16C8H PAL


POWER JC DAUGHTER PCB     K91E0717B  OPERATION TIGER
+----------------------------------------------------------------------------------------------------+
|  |-----------J Connector---------|    |-----------K Connector---------|                            |
|                                                                                                    |
|   IDT74FCT                                                                                         |
|                                                                                                    |
|       IDT74FCT                                                                                     |
|                                                                      LC321664AM                    |
|                                                                                                    |
|   Taito E63-09       E63 17-1              Taito E63-23               E63 27-1                     |
|                                                                                                    |
|                                                                                                    |
|   Taito E63-10       E63 18-1              Taito E63-24    IDT74FCT   E63 28-1                     |
|                                                                                                    |
|                                                                                                    |
|   Taito E63-11        M66220               Taito E63-25     LC3564SM   RESET                       |
|                                                                                                   +-+
|                                                                                                   |C|
|   *                                        Taito E63-26     RTC64613A  C5                         |o|
|                                                                                                   |n|
|                                                                                                   |n| Connects to
|   E63 30-1       E63 32-1                                                             25.0000MHz  |e| filter board
|                                                                           +----------+            |c|
|                                                                           |Toshiba   |            |t|
|   E63 31-1       E63 33-1                                                 |TMP95C063F|            |o|
|                                                                           +----------+ 1.84320MHz |r|
|                                                                                                   +-+
|   Taito E63-15     Taito E63-21                                                           TLP121-4 |
|                                                                                                    |
|                                                                                                    |
|   Taito E63-16     Taito E63-22                                             +----------+  TLP121-4 |
|                                                                             |          |           |
|                                                                             | IDT7024  |           |
|                                                                             |  QFP84   |  TLP121-4 |
|   IDT74FCT      IDT74FCT IDT74FCT                                           |          |           |
|                                                                             +----------+           |
|                                                                                           TLP121-4 |
|                                                                                                    |
|                                                                                                    |
|  |-----------W Connector---------|    |-----------X Connector---------|                            |
+----------------------------------------------------------------------------------------------------+

*  Unpopulated socket for 4th polygon data ROM

OSC1 25.0000MHz
OSC2 1.84320MHz

CPU:
   Toshiba TMP95C063F 16-Bit CMOS Microcontroller (driven by 25.0000MHz OSC)

ROMs:
   8 EPROMs
    - E63 30-1 through E63 32-1 are PowerPC program ROMs
    - E63 17-1 & E63 18-1 are Panasonic MN1020819 program ROMs to drive Zoom chips / sound
    - E63 27-1 & E62 28-1 are Toshiba TMP95C063F program ROMs for I/O
   11 mask ROMs
    - 4 graphics ROMs (E63-15, E63-16, E63-21 & E63-22)
    - 4 Zoom ZSG-2 samples (E63-23 through E63-26)
    - 3 polygon data ROMs (E63-09 through E63-11)

RAM:
   IDT7024 QFP84 4K x 16 Dual-Port SRAM
   Sanyo LC3564BM-70 64K x 8 SRAM
   Sanyo LC321664AM-80 64K x 16 Fast Page DRAM
   M66220FP 256 x 8-bit Dual Port CMOS Mail Box RAM

MISC:
   Reset Push Button
   IDT 74 FCT 16245 CTPA 16-Bit Fast CMOS Bidirection Transceiver (silkscreened FTC16245C)
   Epson RTC64613A 8-Bit Real Time Clock Module with builtin crystal
   C5 is a Super-Cap, supplies back-up power to the RTC
   TLP121-4 Four Channel Photo Transistor, optically coupled to a gallium arsenide InfraRed emitting diode
   J,K, W & X 3x32 pin connector to connect to POWER JC MOTHER-G PCB (Female with KEY receivers)

Note:
 PCB found labeled as K91E0717B came with Operation Tiger Ver 2.14 O
 PCB found labeled as K91E0717A came with Operation Tiger Ver 2.10 O


POWER JC FILTER PCB
+----------------------------------------------------------------------------------------------------+
|                                                                                                    |
|                                                                                                    |
|                                          S......             ......V                               |
|                                           6    1             6    1                                |
|                                                                                *RS                 |
|                                                                                                    |
|                                            111112222233333444445                                   |
|                                        2468024680246802468024680                                   |
|                 P.........            |=========================|G                                 |
|                  1       9                  11111222223333344444                     *MIDI         |
|                                        1357913579135791357913579                                   |
+----------------------------------------------------------------------------------------------------+

*  Unpopulated components

V)ideo Connector, 6 pins    |    S)ound Connector, 6 pins
----------------------------+--------------------------------
   1] Red                   |       1] Sound R(-)
   2] Green                 |       2] Sound R(+)
   3] Blue                  |       3] Sound L(-)
   4] SYNC                  |       4] Sound L(+)
   5] GND                   |       5] W SND (-)  [Subwoofer]
   6] GND                   |       6] W SND (+)  [Subwoofer]


P)ower Connector, 9 pins
------------------------
   1] +12 Volts
   2] +13 Volts
   3] +5 Volts
   4] +5 Volts
   5] +5 Volts
   6] Ground
   7] Ground
   8] Ground
   9] Ground

G)ame Connector, dual row 50 pins
----------------------------------------------
   1] Meter                    2]
   3] Lockout                  4]
   5] 1P Solenoid A            6] 1P Solenoid B
   7] 2P Solenoid A            8] 2P Solenoid B
   9] 1P Start Lamp           10] 2P Start Lamp
  11]                         12]
  13]                         14]
  15]                         16]
  17]                         18]
  19] 1P X Position           20] 1P Y Position
  21] 2P X Position           22] 2P Y Position
  23]                         24]
  25]                         26]
  27]                         28] 1P Trigger
  29]                         30] 1P Bomb
  31]                         32] 2P Trigger
  33] Service                 34] 2P Bomb
  35] Test                    36]
  37]                         38] 1P Start
  39] 2P Start                40] Coin
  41]                         42] +12 Volts
  43] +12 Volts               44] +5 Volts
  45] +5 Volts                46] +5 Volts
  47] Ground                  48] Ground
  49] Ground                  50] Ground

*/

/*
    PPC -> TLCS Commands:
        0x5010:            ?                                        RTC?
        0x5020:            ?                                        RTC?
        0x6000:            ?                                        Backup RAM init?
        0x6010:            ?                                        Backup RAM Read. Address in io_shared[0x1d00].
        0x6020:            ?                                        Backup RAM Write. Address in io_shared[0x1d00].
        0x6030:            ?                                        ?
        0x6040:            ?                                        ?
        0x4000:            ?                                        Sound?
        0x4001:            ?
        0x4002:            ?
        0x4003:            ?
        0x4004:            ?
        0xf055:
        0xf0ff:
        0xf000:
        0xf001:
        0xf010:
        0xf020:

    TLCS -> PPC Commands:
        0x7000:                                                     DSP ready
        0xd000:                                                     Vblank

*/

#include <cmath>

#include "emu.h"

#include "tc0780fpa.h"

#include "cpu/mn10200/mn10200.h"
#include "cpu/powerpc/ppc.h"
#include "cpu/tlcs900/tmp95c063.h"
#include "cpu/tms320c5x/tms320c5x.h"
#include "machine/nvram.h"
#include "sound/zsg2.h"

#include "emupal.h"
#include "screen.h"
#include "speaker.h"
#include "tilemap.h"

#include "endianness.h"


#define LOG_TLCS_TO_PPC_COMMANDS (1U << 1)
#define LOG_PPC_TO_TLCS_COMMANDS (1U << 2)
#define LOG_DISPLAY_LIST         (1U << 3)
#define LOG_VIDEO                (1U << 4)
#define LOG_IO                   (1U << 5)
#define LOG_DSP                  (1U << 6)
#define LOG_SOUND                (1U << 7)

#define VERBOSE (LOG_GENERAL)
#include "logmacro.h"

#define LOGVIDEO(...)  LOGMASKED(LOG_VIDEO, __VA_ARGS__)
#define LOGIO(...)     LOGMASKED(LOG_IO, __VA_ARGS__)
#define LOGDSP(...)    LOGMASKED(LOG_DSP, __VA_ARGS__)
#define LOGSOUND(...)  LOGMASKED(LOG_SOUND, __VA_ARGS__)

namespace {

#define FATAL_UNKNOWN_CALLS (0)

class taitopjc_state : public driver_device
{
public:
	taitopjc_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_iocpu(*this, "iocpu")
		, m_soundcpu(*this, "mn10200")
		, m_dsp(*this, "dsp")
		, m_tc0780fpa(*this, "tc0780fpa")
		, m_zsg2(*this, "zsg2")
		, m_palette(*this, "palette")
		, m_polyrom(*this, "poly")
		, m_gfxdecode(*this, "gfxdecode")
		, m_main_ram(*this, "main_ram")
		, m_dsp_ram(*this, "dsp_ram")
		, m_io_share_ram(*this, "io_share_ram", 0x2000, ENDIANNESS_LITTLE) // or ENDIANNESS_BIG?
		, m_screen_ram(*this, "screen_ram", 0x40000*4, ENDIANNESS_BIG)
		, m_paletteram(*this, "paletteram", 0x8000*4, ENDIANNESS_BIG)
	{ }

	void taitopjc(machine_config &config);

	void init_optiger();

protected:
	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;
	virtual void video_start() override ATTR_COLD;

private:
	required_device<ppc603e_device> m_maincpu;
	required_device<tmp95c063_device> m_iocpu;
	required_device<mn1020819_device> m_soundcpu;
	required_device<cpu_device> m_dsp;
	required_device<tc0780fpa_device> m_tc0780fpa;
	required_device<zsg2_device> m_zsg2;
	required_device<palette_device> m_palette;
	required_memory_region m_polyrom;
	required_device<gfxdecode_device> m_gfxdecode;
	required_shared_ptr<uint64_t> m_main_ram;
	required_shared_ptr<uint16_t> m_dsp_ram;
	memory_share_creator<uint16_t> m_io_share_ram;
	memory_share_creator<uint32_t> m_screen_ram;
	memory_share_creator<uint32_t> m_paletteram;

	uint64_t video_r(offs_t offset, uint64_t mem_mask = ~0);
	void video_w(offs_t offset, uint64_t data, uint64_t mem_mask = ~0);
	uint64_t ppc_common_r(offs_t offset, uint64_t mem_mask = ~0);
	void ppc_common_w(offs_t offset, uint64_t data, uint64_t mem_mask = ~0);
	uint64_t dsp_r(offs_t offset, uint64_t mem_mask = ~0);
	void dsp_w(offs_t offset, uint64_t data, uint64_t mem_mask = ~0);
	uint8_t tlcs_common_r(offs_t offset);
	void tlcs_common_w(offs_t offset, uint8_t data);
	uint8_t tlcs_sound_r(offs_t offset);
	void tlcs_sound_w(offs_t offset, uint8_t data);
	uint16_t zsg2_r(offs_t offset, uint16_t mem_mask = ~0);
	void zsg2_w(offs_t offset, uint16_t data, uint16_t mem_mask = ~0);
	uint8_t snd_port3_r();
	uint8_t snd_shared_ram_r(offs_t offset);
	void snd_shared_ram_w(offs_t offset, uint8_t data);
	TIMER_CALLBACK_MEMBER(snd_reset_release);
	void snd_timer_arm();
	uint8_t tlcs_vol_r(offs_t offset);
	void tlcs_vol_w(offs_t offset, uint8_t data);
	void tlcs_unk_w(offs_t offset, uint16_t data);
	void tms_dspshare_w(offs_t offset, uint16_t data);
	uint16_t dsp_rom_r();
	void dsp_roml_w(uint16_t data);
	void dsp_romh_w(uint16_t data);
	uint32_t screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	INTERRUPT_GEN_MEMBER(vbi);
	uint32_t videochip_r(offs_t address);
	void videochip_w(offs_t address, uint32_t data);
	void video_exit();
	[[maybe_unused]] void print_display_list();
	template <unsigned Offset> TILE_GET_INFO_MEMBER(get_tile_info);

	tilemap_t *m_tilemap[2]{};

	uint32_t m_video_address = 0;

	uint32_t m_dsp_rom_address = 0;
	uint16_t m_scroll_x = 0;
	uint16_t m_scroll_y = 0;

	uint32_t m_tlcs_sound_ptr = 0;
	// M66220FP dual-port RAM shared between the I/O CPU and the MN1020819 sound CPU.
	std::array<uint8_t, 0x100> m_snd_shared_ram{};
	// Sound-board reset / ready-handshake model.  The I/O CPU clears 0x17, strobes
	// the sound board and then spins on "cp (0x040017),0x55"; the MN1020819 answers
	// 0x55 once it has re-initialised the mailbox.  See tlcs_sound_w().
	emu_timer *m_snd_reset_timer = nullptr;
	bool m_snd_force_ready = false;
	uint32_t m_snd_hs_spin = 0;
	// MB87078 volume latch as driven by the I/O CPU (0x04c001 = control, 0x04c000 = gain).
	uint8_t m_vol_control = 0;

	void mn10200_map(address_map &map) ATTR_COLD;
	void ppc603e_mem(address_map &map) ATTR_COLD;
	void tlcs900h_mem(address_map &map) ATTR_COLD;
	void tms_data_map(address_map &map) ATTR_COLD;
	void tms_io_map(address_map &map) ATTR_COLD;
	void tms_program_map(address_map &map) ATTR_COLD;
};

void taitopjc_state::video_exit()
{
#if 0
	FILE *file;
	int i;

	file = fopen("pjc_screen_ram.bin","wb");
	for (i=0; i < 0x40000; i++)
	{
		fputc((uint8_t)(m_screen_ram[i] >> 24), file);
		fputc((uint8_t)(m_screen_ram[i] >> 16), file);
		fputc((uint8_t)(m_screen_ram[i] >> 8), file);
		fputc((uint8_t)(m_screen_ram[i] >> 0), file);
	}
	fclose(file);

	file = fopen("pjc_pal_ram.bin","wb");
	for (i=0; i < 0x8000; i++)
	{
		fputc((uint8_t)(m_pal_ram[i] >> 24), file);
		fputc((uint8_t)(m_pal_ram[i] >> 16), file);
		fputc((uint8_t)(m_pal_ram[i] >> 8), file);
		fputc((uint8_t)(m_pal_ram[i] >> 0), file);
	}
	fclose(file);
#endif
}

template <unsigned Offset>
TILE_GET_INFO_MEMBER(taitopjc_state::get_tile_info)
{
	uint32_t val = m_screen_ram[Offset + (tile_index >> 1)];

	if (BIT(~tile_index, 0))
		val >>= 16;

	int const color = (val >> 12) & 0xf;
	int const tile = (val & 0xfff);

	tileinfo.set(0, tile, color, 0);
}

void taitopjc_state::video_start()
{
	static const gfx_layout char_layout =
	{
		16, 16,
		4096,
		8,
		{ 0,1,2,3,4,5,6,7 },
		{ 3*8, 2*8, 1*8, 0*8, 7*8, 6*8, 5*8, 4*8, 11*8, 10*8, 9*8, 8*8, 15*8, 14*8, 13*8, 12*8 },
		{ 0*128, 1*128, 2*128, 3*128, 4*128, 5*128, 6*128, 7*128, 8*128, 9*128, 10*128, 11*128, 12*128, 13*128, 14*128, 15*128 },
		8*256
	};

	m_tilemap[0] = &machine().tilemap().create(*m_gfxdecode, tilemap_get_info_delegate(*this, FUNC(taitopjc_state::get_tile_info<0x3f000>)), TILEMAP_SCAN_ROWS, 16, 16, 64, 64);
	m_tilemap[1] = &machine().tilemap().create(*m_gfxdecode, tilemap_get_info_delegate(*this, FUNC(taitopjc_state::get_tile_info<0x3f800>)), TILEMAP_SCAN_ROWS, 16, 16, 64, 64);
	m_tilemap[0]->set_transparent_pen(0);
	m_tilemap[1]->set_transparent_pen(0);

	m_gfxdecode->set_gfx(0, std::make_unique<gfx_element>(m_palette, char_layout, (uint8_t*)&m_screen_ram[0], 0, m_palette->entries() / 256, 0));

	m_palette->basemem().set(m_paletteram, m_paletteram.bytes(), 32, ENDIANNESS_BIG, 4);

	save_item(NAME(m_video_address));
	save_item(NAME(m_scroll_x));
	save_item(NAME(m_scroll_y));

	machine().add_notifier(MACHINE_NOTIFY_EXIT, machine_notify_delegate(&taitopjc_state::video_exit, this));
}

uint32_t taitopjc_state::screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	bitmap.fill(0, cliprect);

	m_tc0780fpa->draw(bitmap, cliprect);

	m_tilemap[0]->set_scrollx(m_scroll_x);
	m_tilemap[0]->set_scrolly(m_scroll_y);

	m_tilemap[0]->draw(screen, bitmap, cliprect, 0);
	m_tilemap[1]->draw(screen, bitmap, cliprect, 0);

	return 0;
}

uint32_t taitopjc_state::videochip_r( offs_t address)
{
	uint32_t r = 0;

	if (address >= 0x10000000 && address < 0x10040000)
	{
		r = m_screen_ram[address - 0x10000000];
	}

	return r;
}

void taitopjc_state::videochip_w(offs_t address, uint32_t data)
{
	if (address >= 0x20000000 && address < 0x20008000)
	{
		m_palette->write32(address - 0x20000000, data);
	}
	else if (address >= 0x10000000 && address < 0x10040000)
	{
		uint32_t const addr = address - 0x10000000;
		m_screen_ram[addr] = data;

		if (address >= 0x1003f000 && address < 0x10040000)
		{
			uint32_t const a = address & 0x7ff;
			m_tilemap[BIT(address, 11)]->mark_tile_dirty((a * 2) + 0);
			m_tilemap[BIT(address, 11)]->mark_tile_dirty((a * 2) + 1);
		}
		m_gfxdecode->gfx(0)->mark_dirty(addr / 64);
	}
	else if (address == 0x00000006)
	{
		m_scroll_y = (data >> 16) & 0xffff;
		m_scroll_x = data & 0xffff;
	}
	else if (address < 6)
	{
		// [0] hi <unused>?
		// [0] lo VTOTAL
		// [1] hi VDISPLAY end
		// [1] lo VDISPLAY start
		// [2] hi VSYNC start
		// [2] lo VSYNC end
		// [3-5] same but for Horizontal, in pixel units
		LOG("CRTC address %d %08x (U: %d L: %d)\n", address, data, data >> 16, data & 0xffff);
	}
	else
	{
		LOGVIDEO("Video Address %08X = %08X\n", address, data);
	}
}

uint64_t taitopjc_state::video_r(offs_t offset, uint64_t mem_mask)
{
	uint64_t r = 0;

	if (offset == 0)
	{
		if (ACCESSING_BITS_32_63)
		{
			r |= (uint64_t)(videochip_r(m_video_address)) << 32;
		}
	}

	return r;
}

void taitopjc_state::video_w(offs_t offset, uint64_t data, uint64_t mem_mask)
{
	if (offset == 0)
	{
		if (ACCESSING_BITS_32_63)
		{
			LOGVIDEO("Video Address %08X = %08X\n", m_video_address, (uint32_t)(data >> 32));
			videochip_w(m_video_address, (uint32_t)(data >> 32));
		}
	}
	if (offset == 1)
	{
		if (ACCESSING_BITS_32_63)
		{
			m_video_address = (uint32_t)(data >> 32);
		}
	}
}

uint64_t taitopjc_state::ppc_common_r(offs_t offset, uint64_t mem_mask)
{
	uint64_t r = 0;

	LOGIO("ppc_common_r: %08X, %016X\n", offset, mem_mask);

	uint32_t const address = offset * 2;

	if (ACCESSING_BITS_48_63)
	{
		r |= uint64_t(m_io_share_ram[address]) << 48;
	}
	if (ACCESSING_BITS_16_31)
	{
		r |= uint64_t(m_io_share_ram[address + 1]) << 16;
	}

	return r;
}

void taitopjc_state::ppc_common_w(offs_t offset, uint64_t data, uint64_t mem_mask)
{
	uint32_t const address = offset * 2;

	LOGIO("ppc_common_w: %08X, %X, %X\n", offset, data, mem_mask);

	if (ACCESSING_BITS_48_63)
	{
		m_io_share_ram[address] = (uint16_t)(data >> 48);
	}
	if (ACCESSING_BITS_16_31)
	{
		m_io_share_ram[address + 1] = (uint16_t)(data >> 16);
	}

	// Command hand-off to the I/O CPU.
	//
	// The command word lives in m_io_share_ram[0xfff], which this handler fills from
	// ACCESSING_BITS_16_31 (address + 1 = 0xfff when offset == 0x7ff).  The guard used
	// to require ACCESSING_BITS_48_63, which fills 0xffe instead, so it never matched
	// the transfer that actually carries the command: a runtime probe shows the PPC
	// sending 0x6010 with mem_mask 0x00000000ffff0000 (bits 16-31) over and over while
	// the interrupt was never raised, so the I/O CPU never serviced a single command.
	// Accept either halfword so the hand-off fires whenever this word is written.
	if (offset == 0x7ff && (ACCESSING_BITS_48_63 || ACCESSING_BITS_16_31))
	{
		if (m_io_share_ram[0xfff] != 0x0000)
		{
			LOGMASKED(LOG_PPC_TO_TLCS_COMMANDS, "PPC -> TLCS cmd %04X\n", m_io_share_ram[0xfff]);
		}

		m_iocpu->set_input_line(TLCS900_INT6, ASSERT_LINE);
		m_maincpu->set_input_line(INPUT_LINE_IRQ0, CLEAR_LINE);

		m_maincpu->abort_timeslice();

	}
}

uint64_t taitopjc_state::dsp_r(offs_t offset, uint64_t mem_mask)
{
	uint64_t r = 0;

	if (ACCESSING_BITS_48_63)
	{
		int const addr = offset * 2;
		r |= (uint64_t)(m_dsp_ram[addr + 0]) << 48;
	}
	if (ACCESSING_BITS_16_31)
	{
		int const addr = offset * 2;
		r |= (uint64_t)(m_dsp_ram[addr + 1]) << 16;
	}

	return r;
}

void taitopjc_state::print_display_list()
{
	int ptr = 0;

	uint16_t const cmd = m_dsp_ram[0xffe];
	if (cmd == 0x5245)
	{
		logerror("DSP command RE\n");
		bool end = false;
		do
		{
			uint16_t const w = m_dsp_ram[ptr++];
			if (BIT(w, 15))
			{
				int const count = (w & 0x7fff) + 1;
				uint16_t d = m_dsp_ram[ptr++];
				for (int i = 0; i < count; i++)
				{
					uint16_t const s = m_dsp_ram[ptr++];
					logerror("   %04X -> [%04X]\n", s, d);
					d++;
				}
			}
			else if (w == 0)
			{
				end = true;
			}
			else
			{
				switch (w)
				{
					case 0x406d:
						logerror("   Call %04X [%04X %04X]\n", w, m_dsp_ram[ptr], m_dsp_ram[ptr+1]);
						ptr += 2;
						break;
					case 0x40cd:
						logerror("   Call %04X [%04X %04X]\n", w, m_dsp_ram[ptr], m_dsp_ram[ptr+1]);
						ptr += 2;
						break;
					case 0x40ac:
						logerror("   Call %04X [%04X %04X %04X %04X %04X %04X %04X %04X]\n", w, m_dsp_ram[ptr], m_dsp_ram[ptr+1], m_dsp_ram[ptr+2], m_dsp_ram[ptr+3], m_dsp_ram[ptr+4], m_dsp_ram[ptr+5], m_dsp_ram[ptr+6], m_dsp_ram[ptr+7]);
						ptr += 8;
						break;
					case 0x4774:
						logerror("   Call %04X [%04X %04X %04X]\n", w, m_dsp_ram[ptr], m_dsp_ram[ptr+1], m_dsp_ram[ptr+2]);
						ptr += 3;
						break;
					case 0x47d9:
						logerror("   Call %04X [%04X %04X %04X %04X %04X %04X %04X %04X]\n", w, m_dsp_ram[ptr], m_dsp_ram[ptr+1], m_dsp_ram[ptr+2], m_dsp_ram[ptr+3], m_dsp_ram[ptr+4], m_dsp_ram[ptr+5], m_dsp_ram[ptr+6], m_dsp_ram[ptr+7]);
						ptr += 8;
						break;
					default:
					{
						logerror("Unknown call %04X\n", w);
						for (int i = 0; i < 10; i++)
						{
							logerror("%04X\n", m_dsp_ram[ptr++]);
						}
						if (FATAL_UNKNOWN_CALLS)
							fatalerror("Unknown call %04X\n", w);
						break;
					}
				}
			}
		} while(!end);
	}
	else
	{
		if (cmd != 0)
			logerror("DSP command %04X\n", cmd);
		return;
	}
}

void taitopjc_state::dsp_w(offs_t offset, uint64_t data, uint64_t mem_mask)
{
	LOGDSP("dsp_w: %08X, %08X%08X, %08X%08X at %08X\n", offset, (uint32_t)(data >> 32), (uint32_t)(data), (uint32_t)(mem_mask >> 32), (uint32_t)(mem_mask), m_maincpu->pc());

	if (offset == 0x7fe)
	{
		#if 0
		{
			FILE *f = fopen("dspram.bin", "wb");
			for (int i = 0; i < 0x1000; i++)
			{
				fputc((dsp_ram[i] >> 0) & 0xff, f);
				fputc((dsp_ram[i] >> 8) & 0xff, f);
			}
			fclose(f);
		}
		#endif

	// TODO: convert to debugger command
#if (VERBOSE & LOG_DISPLAY_LIST)
		print_display_list();
#endif
	}

	if (ACCESSING_BITS_48_63)
	{
		int const addr = offset * 2;
		m_dsp_ram[addr + 0] = (data >> 48) & 0xffff;
	}
	if (ACCESSING_BITS_16_31)
	{
		int const addr = offset * 2;
		m_dsp_ram[addr + 1] = (data >> 16) & 0xffff;
	}
}

// BAT Config:
// IBAT0 U: 0x40000002   L: 0x40000022      (0x40000000...0x4001ffff)
// IBAT1 U: 0x0000007f   L: 0x00000002      (0x00000000...0x003fffff)
// IBAT2 U: 0xc0000003   L: 0xc0000022      (0xc0000000...0xc001ffff)
// IBAT3 U: 0xfe0003ff   L: 0xfe000022      (0xfe000000...0xffffffff)
// DBAT0 U: 0x40000002   L: 0x40000022      (0x40000000...0x4001ffff)
// DBAT1 U: 0x0000007f   L: 0x00000002      (0x00000000...0x003fffff)
// DBAT2 U: 0xc0000003   L: 0xc0000022      (0xc0000000...0xc001ffff)
// DBAT3 U: 0xfe0003ff   L: 0xfe000022      (0xfe000000...0xffffffff)

void taitopjc_state::ppc603e_mem(address_map &map)
{
	map(0x00000000, 0x003fffff).ram().share(m_main_ram); // Work RAM
	map(0x40000000, 0x4000000f).rw(FUNC(taitopjc_state::video_r), FUNC(taitopjc_state::video_w));
	map(0x80000000, 0x80003fff).rw(FUNC(taitopjc_state::dsp_r), FUNC(taitopjc_state::dsp_w));
	map(0xc0000000, 0xc0003fff).rw(FUNC(taitopjc_state::ppc_common_r), FUNC(taitopjc_state::ppc_common_w));
	map(0xfe800000, 0xff7fffff).rom().region("maingfx", 0);
	map(0xffe00000, 0xffffffff).rom().region("maindata", 0);
}




uint8_t taitopjc_state::tlcs_common_r(offs_t offset)
{
	if (BIT(offset, 0))
	{
		return (uint8_t)(m_io_share_ram[offset / 2] >> 8);
	}
	else
	{
		return (uint8_t)(m_io_share_ram[offset / 2]);
	}
}

void taitopjc_state::tlcs_common_w(offs_t offset, uint8_t data)
{
	if (BIT(offset, 0))
	{
		m_io_share_ram[offset / 2] &= 0x00ff;
		m_io_share_ram[offset / 2] |= (uint16_t)(data) << 8;
	}
	else
	{
		m_io_share_ram[offset / 2] &= 0xff00;
		m_io_share_ram[offset / 2] |= data;
	}

	if (offset == 0x1fff)
	{
		m_iocpu->set_input_line(TLCS900_INT6, CLEAR_LINE);
	}

	if (offset == 0x1ffd)
	{
		if (m_io_share_ram[0xffe] != 0xd000 &&
			m_io_share_ram[0xffe] != 0x7000)
		{
			LOGMASKED(LOG_TLCS_TO_PPC_COMMANDS, "TLCS -> PPC cmd %04X\n", m_io_share_ram[0xffe]);
		}

		if (m_io_share_ram[0xffe] == 0xd000)
			m_iocpu->set_input_line(TLCS900_INT1, CLEAR_LINE);
		if (m_io_share_ram[0xffe] == 0x7000)
			m_iocpu->set_input_line(TLCS900_INT2, CLEAR_LINE);

		if (m_io_share_ram[0xffe] != 0)
			m_maincpu->set_input_line(INPUT_LINE_IRQ0, ASSERT_LINE);


		m_iocpu->abort_timeslice();
	}
}

uint8_t taitopjc_state::tlcs_sound_r(offs_t offset)
{
	// M66220FP dual-port RAM as seen by the I/O CPU.  Every byte is real RAM now and
	// the sound CPU maintains the handshake bytes itself:
	//   0x15  read/consume pointer, written by the sound program at 0x085a87
	//   0x16  sound-board version byte, written at 0x085874
	//   0x17  ready flag, written as 0x55 at 0x08587b once the mailbox is cleared
	//   0x2c  status byte
	//   0x80..0xff  128-byte command ring, filled by the I/O CPU in 4-byte packets
	//                (the write index itself lives in I/O CPU RAM at 0x010046)
	// Reading the ring does NOT advance anything: the I/O CPU only ever reads it
	// back during its own RAM test.
	uint8_t const data = m_snd_shared_ram[offset & 0xff];

	// Watchdog: the I/O CPU spins on "cp (0x040017),0x55" forever if the sound CPU
	// never gets there.  Rather than hang the whole machine, give up after a while
	// and say so - that keeps the rest of the game observable while the sound board
	// is still being brought up.
	if (offset == 0x17)
	{
		if (data == 0x55)
			m_snd_hs_spin = 0;
		else if (m_snd_force_ready)
			return 0x55;
		else if (++m_snd_hs_spin > 8000000)
		{
			m_snd_force_ready = true;
			logerror("taitopjc: sound board never asserted 0x55 at 0x040017, forcing handshake\n");
			return 0x55;
		}
	}

	return data;
}

void taitopjc_state::tlcs_sound_w(offs_t offset, uint8_t data)
{
	// The other port of the same dual-port RAM.  A write into the 0x80-0xff half is
	// what the M66220FP turns into an interrupt on the sound CPU side.
	//
	// 0x040017 = 0 starts the sound-board reset handshake (0xfc1810 / 0xfc1853): the
	// I/O CPU clears the flag, strobes the board (SFR 0x18 bit 0), runs the mailbox
	// RAM test over 0x040040-0x0400ff and then spins waiting for 0x55.  Hold the
	// sound CPU in reset across that test so the two sides cannot corrupt each
	// other's mailbox traffic, then release it - the sound program clears the whole
	// mailbox and writes 0x55 back, which completes the handshake.
	if (offset == 0x17 && data == 0x00 && !machine().side_effects_disabled())
	{
		m_snd_shared_ram[0x17] = 0x00;
		m_snd_force_ready = false;
		m_snd_hs_spin = 0;
		m_soundcpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
		m_snd_reset_timer->adjust(attotime::from_msec(2));
		return;
	}

	LOGSOUND("tlcs_sound_w: %08X, %02X\n", offset, data);
	m_snd_shared_ram[offset & 0xff] = data;

	if (offset >= 0x80 && offset < 0x100)
	{
		// The M66220FP raises an interrupt on the sound CPU side when the I/O CPU
		// writes the mailbox, and the flag stays asserted until the sound CPU reads
		// it.  Model that as a level on IRQ0: assert when a packet starts (the send
		// routine writes the first byte at 0x80), release when the sound CPU reads
		// the ring (see snd_shared_ram_r).  The previous code poked 0xfc51 with 0x1f
		// on every ring byte - 0xfc51 is an odd address the CPU core does not even
		// implement, and the pulse was released in the same step.
		if (offset == 0x80 && data != 0x00)
			m_soundcpu->set_input_line(0, ASSERT_LINE);
	}
}

// ZSG-2 sample-PCM chip.
//
// The address map offset is passed to the device UNCHANGED.  A runtime probe proves
// this is what the hardware expects: the engine initialises the chip with writes to
// 0x304-0x314 (data 0xffff to key-off 0x04-0x06, 0x5cbc to 0x0c/0x0d, 0x0128 to
// 0x10, 0x0066 to 0x14), and zsg2_device subtracts 0x300 from anything at or above
// it to obtain the control register index - so 0x310 becomes control 0x10, exactly
// as intended.  ZN uses the identical offsets for the same chip, which confirms the
// encoding.  An earlier attempt to halve the offset was wrong: it turned 0x310 into
// channel 24 register 8 and every control write was lost.
uint16_t taitopjc_state::zsg2_r(offs_t offset, uint16_t mem_mask)
{
	uint16_t const result = m_zsg2->read(offset, mem_mask);
	return result;
}

void taitopjc_state::zsg2_w(offs_t offset, uint16_t data, uint16_t mem_mask)
{

	m_zsg2->write(offset, data, mem_mask);
}

// MN1020819 port 3: sound-board ID straps.
uint8_t taitopjc_state::snd_port3_r()
{
	return 0x00;
}

// Same dual-port RAM, seen from the sound CPU side (also 0x040000 on this bus).
uint8_t taitopjc_state::snd_shared_ram_r(offs_t offset)
{
	offset &= 0xff;

	// Reading the command ring acknowledges the mailbox interrupt (the M66220FP
	// clears its IRQ flag when the sound CPU touches its side of the window).
	if (offset >= 0x80 && !machine().side_effects_disabled())
		m_soundcpu->set_input_line(0, CLEAR_LINE);

	return m_snd_shared_ram[offset];
}

void taitopjc_state::snd_shared_ram_w(offs_t offset, uint8_t data)
{
	offset &= 0xff;
	m_snd_shared_ram[offset] = data;

	// 0x17 is the sound-board ready flag.  The sound program clears the complete
	// mailbox and then writes 0x55 here (0x08587b); that is exactly what the I/O CPU
	// spins on at 0xfc1847 / 0xfc1877.
	if (offset == 0x17 && data == 0x55)
	{
		m_snd_force_ready = false;
		m_snd_hs_spin = 0;
		logerror("taitopjc: sound board ready (version 0x%02x)\n", m_snd_shared_ram[0x16]);
	}

}

//-------------------------------------------------
TIMER_CALLBACK_MEMBER(taitopjc_state::snd_reset_release)
{
	m_soundcpu->set_input_line(INPUT_LINE_RESET, CLEAR_LINE);
	snd_timer_arm();
	// The board loads the ZFX-2 once at boot; do the same here so the DSP runs its
	// microcode instead of sitting idle.
}

// EXPERIMENT: the sound program expects its tick timer to run from registers the core
// does not model (0xfff0-0xfff6).  Enable the core's prescaler 0 so the forwarded
// timer writes below actually produce interrupts.
void taitopjc_state::snd_timer_arm()
{
	m_soundcpu->io_control_w(0x21a, 0x00);  // prescaler 0 base
	m_soundcpu->io_control_w(0x22a, 0x80);  // prescaler 0 mode: enabled
}

//-------------------------------------------------
//  MB87078 electronic volume
//-------------------------------------------------
//
// Two byte latches on the I/O CPU bus, driven by the boot routines at 0xfc176c /
// 0xfc1782 / 0xfc1798 (channels 0/1/3) and by PPC command 0x4002, which supplies
// the three levels from 0x061c04 / 0x061c06 / 0x061c0c:
//
//   0x04c001 = control: bits 0-1 = channel, bit 2 = EN (must be set to pass audio)
//   0x04c000 = 6-bit gain code
//
// The chip sits next to the uDP6379 DAC and feeds the filter board's left, right
// and subwoofer outputs, so channel 0/1 are applied to the ZFX-2/ZSG-2 outputs.
uint8_t taitopjc_state::tlcs_vol_r(offs_t offset)
{
	return (offset & 1) ? m_vol_control : 0x3f;
}

void taitopjc_state::tlcs_vol_w(offs_t offset, uint8_t data)
{
	if (offset & 1)
	{
		// channel select + EN
		m_vol_control = data;
		return;
	}

	// 6-bit gain code latched for the selected channel.  The chip is a 64-step
	// attenuator: code 63 = 0dB, 62 = -0.5dB, ... 0 = -31.5dB, and EN low = mute.
	// Rendered inline because this tree builds with a device whitelist that does not
	// carry mb87077.cpp; swapping in mb87078_device later is a drop-in change.
	int const channel = m_vol_control & 0x03;
	float gain = 0.0f;
	if ((m_vol_control & 0x04) && (data & 0x3f))
		gain = float(pow(10.0, -0.5 * (63 - (data & 0x3f)) / 20.0));

	// Channels 0/1 drive the filter board's left/right pair; channel 3 is the
	// subwoofer feed, which is not routed yet.
	if (channel == 0)
	{
		m_zsg2->set_output_gain(2, gain);
	}
	else if (channel == 1)
	{
		m_zsg2->set_output_gain(3, gain);
	}
}

void taitopjc_state::tlcs_unk_w(offs_t offset, uint16_t data)
{
	if (offset == 0xc/2)
	{
		m_maincpu->set_input_line(INPUT_LINE_RESET, BIT(data, 2));
	}
}

// TLCS900 interrupt vectors
// 0xfc0100: reset
// 0xfc00ea: INT0 (dummy)
// 0xfc00eb: INT1 vblank?
// 0xfc00f0: INT2 DSP ready?
// 0xfc00f5: INT3 (dummy)
// 0xfc00f6: INT4 (dummy)
// 0xfc00f7: INT5 (dummy)
// 0xfc00f8: INT6 PPC command
// 0xfc00fd: INT7 (dummy)
// 0xfc00fe: int8_t (dummy)
// 0xfc0663: INTT1
// 0xfc0f7d: INTRX0
// 0xfc0f05: INTTX0
// 0xfc0fb5: INTRX1
// 0xfc0f41: INTTX1

void taitopjc_state::tlcs900h_mem(address_map &map)
{
	map(0x010000, 0x02ffff).ram(); // Work RAM
	map(0x040000, 0x0400ff).rw(FUNC(taitopjc_state::tlcs_sound_r), FUNC(taitopjc_state::tlcs_sound_w));
	map(0x044000, 0x045fff).ram().share("nvram");
	// MB87078 electronic volume: 0x04c001 = control, 0x04c000 = gain code.
	map(0x04c000, 0x04c001).rw(FUNC(taitopjc_state::tlcs_vol_r), FUNC(taitopjc_state::tlcs_vol_w));
	map(0x060000, 0x061fff).rw(FUNC(taitopjc_state::tlcs_common_r), FUNC(taitopjc_state::tlcs_common_w));
	map(0x06c000, 0x06c00f).w(FUNC(taitopjc_state::tlcs_unk_w));
	map(0xfc0000, 0xffffff).rom().region("iocpu", 0);
}

void taitopjc_state::mn10200_map(address_map &map)
{
	// The sound program keeps its stack (SP = 0xFBFC) and all of its variables in
	// the low 64K, so this RAM is mandatory: without it every stack access reads
	// open bus and the CPU never gets past its boot code.
	map(0x000000, 0x00fbff).ram();     // work RAM
	// 0x00fc00-0x00ffff is the MN10200 internal I/O block, which the CPU core maps
	// itself; internal maps are applied after the driver map and therefore win.  The
	// two unmodelled port bytes used by the ZFX-2 bit-serial link are hooked from
	// machine_start() with runtime handlers instead (see snd_ser_clk_w).

	map(0x080000, 0x0fffff).rom().region("mn10200", 0);

	// ZOOM ZSG-2 sample PCM.  This is the chip's real window, and it was deduced from
	// the engine itself: the sound program programs control registers 0x618/0x61a
	// (0x5cbc), 0x620 (0x0128), 0x628 (0x0066), the key-off flags at 0x608/0x60a/0x60c
	// and performs the sample-ROM readback handshake at 0x638/0x63a/0x63c/0x63e -
	// all of which are absolute references to 0x4006xx.  zsg2.cpp documents exactly
	// those byte offsets for the control block (0x600 = key-on, 0x608 = key-off,
	// 0x638/0x63a = readback address, 0x63c/0x63e = readback data).
	map(0x400000, 0x4007ff).rw(FUNC(taitopjc_state::zsg2_r), FUNC(taitopjc_state::zsg2_w));

	// Work RAM above the ZSG-2 window; holds the engine's variables and block-copy
	// descriptors.
	map(0x400800, 0x41ffff).ram();

	// The engine stages a 16K block here (source 0x087F3E .. 0x08BF3D recorded at
	// 0x801236, copied by the loop at 0x080712-0x08072d) and the boot code blits a
	// 70-byte table to the same base at 0x801fe.  This is ordinary RAM, NOT the
	// sound chip - treating it as the ZSG-2 was an earlier mistake.
	map(0x800000, 0x817fff).ram();
	// Staging buffers the two block-copy loops write into.  The first loop fills
	// 0x818000-0x81bfff and the second continues from there, so the region has to run
	// past 0x81c000 or those writes fall off the map.  Plain RAM, not the sound chip.
	map(0x818000, 0x83ffff).ram();
	// Scratch page the sound program pokes during initialisation.
	map(0x04c000, 0x04c0ff).ram();


	// 32-bit shadow latches the boot code pushes ZFX-2 control words into
	// (0x0858a5-0x08593d).  Kept as plain storage so the read-back sees what was
	// written; the serial link above is what actually feeds the DSP.
	map(0x600000, 0x600003).ram();
	map(0x610000, 0x610003).ram();

	// M66220FP dual-port RAM shared with the I/O CPU: the sound CPU's command
	// mailbox.  A full scan of the sound program shows every mailbox access is an
	// absolute reference into 0x040000-0x0400ff (0x040000, 0x040001, 0x040006-0x04000b,
	// 0x040015-0x040017, 0x04002c) and NONE into 0xe00000, so on this side of the bus
	// the chip answers at 0x040000.  Mapping it anywhere else leaves the sound program
	// reading and writing empty RAM, which is why no command ever got through.
	map(0x040000, 0x0400ff).rw(FUNC(taitopjc_state::snd_shared_ram_r), FUNC(taitopjc_state::snd_shared_ram_w));
}



void taitopjc_state::tms_dspshare_w(offs_t offset, uint16_t data)
{
	if (offset == 0xffc)
	{
		m_iocpu->set_input_line(TLCS900_INT2, ASSERT_LINE);
	}
	m_dsp_ram[offset] = data;
}

uint16_t taitopjc_state::dsp_rom_r()
{
	assert(m_dsp_rom_address < 0x800000);

	uint16_t const data = ((uint16_t*)m_polyrom->base())[m_dsp_rom_address];
	if (!machine().side_effects_disabled())
		m_dsp_rom_address++;
	return data;
}

void taitopjc_state::dsp_roml_w(uint16_t data)
{
	m_dsp_rom_address &= 0xffff0000;
	m_dsp_rom_address |= data;
}

void taitopjc_state::dsp_romh_w(uint16_t data)
{
	m_dsp_rom_address &= 0xffff;
	m_dsp_rom_address |= (uint32_t)(data) << 16;
}


void taitopjc_state::tms_program_map(address_map &map)
{
	map(0x0000, 0x3fff).rom().region("dspdata", 0);
	map(0x4c00, 0xefff).rom().region("dspdata", 0x9800);
}

void taitopjc_state::tms_data_map(address_map &map)
{
	map(0x4000, 0x6fff).rom().region("dspdata", 0x8000);
	map(0x7000, 0xefff).ram();
	map(0xf000, 0xffff).ram().w(FUNC(taitopjc_state::tms_dspshare_w)).share(m_dsp_ram);
}

void taitopjc_state::tms_io_map(address_map &map)
{
	map(0x0053, 0x0053).w(FUNC(taitopjc_state::dsp_roml_w));
	map(0x0057, 0x0057).w(FUNC(taitopjc_state::dsp_romh_w));
	map(0x0058, 0x0058).w(m_tc0780fpa, FUNC(tc0780fpa_device::poly_fifo_w));
	map(0x005a, 0x005a).w(m_tc0780fpa, FUNC(tc0780fpa_device::tex_w));
	map(0x005b, 0x005b).rw(m_tc0780fpa, FUNC(tc0780fpa_device::tex_addr_r), FUNC(tc0780fpa_device::tex_addr_w));
	map(0x005e, 0x005e).noprw(); // ?? 0x0001 written every frame
	map(0x005f, 0x005f).r(FUNC(taitopjc_state::dsp_rom_r));
}


static INPUT_PORTS_START( taitopjc )
	PORT_START("INPUTS1")
	PORT_BIT( 0x00000001, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x00000002, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x00000004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x00000008, IP_ACTIVE_LOW, IPT_COIN1 ) // Coin A
	PORT_BIT( 0x00000010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x00000020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x00000040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x00000080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("INPUTS2")
	PORT_BIT( 0x00000001, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME("Service") PORT_CODE(KEYCODE_7) // Service switch
	PORT_SERVICE_NO_TOGGLE( 0x00000002, IP_ACTIVE_LOW) // Test Button
	PORT_BIT( 0x00000004, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x00000008, IP_ACTIVE_LOW, IPT_START1 ) // Select 1
	PORT_BIT( 0x00000010, IP_ACTIVE_LOW, IPT_START2 ) // Select 2
	PORT_BIT( 0x00000020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x00000040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x00000080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("INPUTS3")
	PORT_BIT( 0x00000001, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1) // P1 trigger
	PORT_BIT( 0x00000002, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1) // P1 bomb
	PORT_BIT( 0x00000004, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2) // P2 trigger
	PORT_BIT( 0x00000008, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2) // P2 bomb
	PORT_BIT( 0x00000010, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x00000020, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x00000040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x00000080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START("ANALOG1") // Player 1 X
	PORT_BIT(0x3ff, 0x200, IPT_AD_STICK_X) PORT_CROSSHAIR(X, -1.0, 0.0, 0) PORT_MINMAX(0x000, 0x3ff) PORT_SENSITIVITY(50) PORT_KEYDELTA(10) PORT_PLAYER(1) PORT_REVERSE

	PORT_START("ANALOG2") // Player 1 Y
	PORT_BIT(0x3ff, 0x200, IPT_AD_STICK_Y) PORT_CROSSHAIR(Y, 1.0, 0.0, 0) PORT_MINMAX(0x000, 0x3ff) PORT_SENSITIVITY(50) PORT_KEYDELTA(10) PORT_PLAYER(1)

	PORT_START("ANALOG3") // Player 2 X
	PORT_BIT(0x3ff, 0x200, IPT_AD_STICK_X) PORT_CROSSHAIR(X, -1.0, 0.0, 0) PORT_MINMAX(0x000, 0x3ff) PORT_SENSITIVITY(50) PORT_KEYDELTA(10) PORT_PLAYER(2) PORT_REVERSE

	PORT_START("ANALOG4") // Player 2 Y
	PORT_BIT(0x3ff, 0x200, IPT_AD_STICK_Y) PORT_CROSSHAIR(Y, 1.0, 0.0, 0) PORT_MINMAX(0x000, 0x3ff) PORT_SENSITIVITY(50) PORT_KEYDELTA(10) PORT_PLAYER(2)
INPUT_PORTS_END


void taitopjc_state::machine_start()
{
	// Keep the DRC's fast configuration; the game needs it to keep up (the compatible
	// set enables strict verify, PC flushes and accurate single-precision rounding,
	// which exist to validate the recompiler rather than to run the game).
	m_maincpu->ppcdrc_set_options(PPCDRC_FASTEST_OPTIONS);

	m_maincpu->ppcdrc_add_fastram(0x00000000, 0x003fffff, false, m_main_ram);

	m_maincpu->ppcdrc_add_fastram(0x00000000, 0x003fffff, false, m_main_ram);

	// EXPERIMENT: map the sound program's timer registers (0xfff0-0xfff6, which this
	// core does not model) onto the core's timer 4-6 block (mode 0x224+i, base
	// 0x214+i) and force the prescaler on, so we can see whether the missing tick is
	// what blocks the sound CPU.  Forwarded writes arrive here one byte at a time.
	auto snd_timer_w = [this](offs_t base, uint8_t addr_low, uint8_t data)
	{
		if (addr_low >= 0xf0 && addr_low <= 0xf2)
			m_soundcpu->io_control_w(0x224 + (addr_low - 0xf0), data);   // mode
		else if (addr_low >= 0xf4 && addr_low <= 0xf6)
			m_soundcpu->io_control_w(0x214 + (addr_low - 0xf4), data);   // base
	};

	m_soundcpu->space(AS_PROGRAM).install_write_tap(0x00fff0, 0x00fff1, "pjc_tmr0",
		[this, snd_timer_w](offs_t offset, uint16_t &data, uint16_t mask)
		{
			if (mask & 0x00ff) snd_timer_w(0xfff0, 0xf0, uint8_t(data & 0xff));
			if (mask & 0xff00) snd_timer_w(0xfff0, 0xf1, uint8_t(data >> 8));
		});
	m_soundcpu->space(AS_PROGRAM).install_write_tap(0x00fff2, 0x00fff3, "pjc_tmr1",
		[this, snd_timer_w](offs_t offset, uint16_t &data, uint16_t mask)
		{
			if (mask & 0x00ff) snd_timer_w(0xfff2, 0xf2, uint8_t(data & 0xff));
		});
	m_soundcpu->space(AS_PROGRAM).install_write_tap(0x00fff4, 0x00fff5, "pjc_tmr2",
		[this, snd_timer_w](offs_t offset, uint16_t &data, uint16_t mask)
		{
			if (mask & 0x00ff) snd_timer_w(0xfff4, 0xf4, uint8_t(data & 0xff));
			if (mask & 0xff00) snd_timer_w(0xfff4, 0xf5, uint8_t(data >> 8));
		});
	m_soundcpu->space(AS_PROGRAM).install_write_tap(0x00fff6, 0x00fff7, "pjc_tmr3",
		[this, snd_timer_w](offs_t offset, uint16_t &data, uint16_t mask)
		{
			if (mask & 0x00ff) snd_timer_w(0xfff6, 0xf6, uint8_t(data & 0xff));
		});

	m_snd_reset_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(taitopjc_state::snd_reset_release), this));

	save_item(NAME(m_dsp_rom_address));
	save_item(NAME(m_tlcs_sound_ptr));
	save_item(NAME(m_snd_shared_ram));
	save_item(NAME(m_snd_force_ready));
	save_item(NAME(m_snd_hs_spin));
	save_item(NAME(m_vol_control));
}


void taitopjc_state::machine_reset()
{
	// The sound CPU must be RELEASED, not held in reset.  The original driver
	// asserted INPUT_LINE_RESET here with the comment "halt sound CPU since we don't
	// emulate this yet", which is exactly why the ZSG-2 never received a single
	// register write: mn10200_device::execute_run() is never entered while the reset
	// line is asserted, so the sound program never ran at all.  Its I/O writes to
	// 0xfcxx during reset are what produced the stray "MN ICRH" messages.
	m_soundcpu->set_input_line(INPUT_LINE_RESET, CLEAR_LINE);

	m_maincpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);

	m_dsp_rom_address = 0;

	m_snd_shared_ram.fill(0);
	m_snd_force_ready = false;
	m_snd_hs_spin = 0;
	m_vol_control = 0;

	// EXPERIMENT: device resets have just cleared the CPU's internal I/O, so arm the
	// prescaler the forwarded timer writes need.
	snd_timer_arm();
}


INTERRUPT_GEN_MEMBER(taitopjc_state::vbi)
{
	m_iocpu->set_input_line(TLCS900_INT1, ASSERT_LINE);
}


void taitopjc_state::taitopjc(machine_config &config)
{
	PPC603E(config, m_maincpu, 100000000);
	m_maincpu->set_bus_frequency(XTAL(66'666'700)); // Multiplier 1.5, Bus = 66MHz, Core = 100MHz
	m_maincpu->set_addrmap(AS_PROGRAM, &taitopjc_state::ppc603e_mem);

	// TMP95C063F I/O CPU
	TMP95C063(config, m_iocpu, 25_MHz_XTAL);
	m_iocpu->port5_read().set_ioport("INPUTS1");
	m_iocpu->portd_read().set_ioport("INPUTS2");
	m_iocpu->porte_read().set_ioport("INPUTS3");
	m_iocpu->an_read<0>().set_ioport("ANALOG1");
	m_iocpu->an_read<1>().set_ioport("ANALOG2");
	m_iocpu->an_read<2>().set_ioport("ANALOG3");
	m_iocpu->an_read<3>().set_ioport("ANALOG4");
	m_iocpu->set_addrmap(AS_PROGRAM, &taitopjc_state::tlcs900h_mem);
	m_iocpu->set_vblank_int("screen", FUNC(taitopjc_state::vbi));

	// TMS320C53 DSP
	TMS320C53(config, m_dsp, 40_MHz_XTAL); // 80MHz rated part, should be 40.0000MHz x 2?
	m_dsp->set_addrmap(AS_PROGRAM, &taitopjc_state::tms_program_map);
	m_dsp->set_addrmap(AS_DATA, &taitopjc_state::tms_data_map);
	m_dsp->set_addrmap(AS_IO, &taitopjc_state::tms_io_map);

	// MN1020819DA sound CPU
	//
	// The music tempo is driven by the tick counter, which only advances once per
	// serviced interrupt.  The timer fires far faster than the CPU can service it, so
	// the effective tick rate (and therefore the music speed) is set by the interrupt
	// servicing throughput.  A prescaler scale of 2 halves the tick rate to ~2.6 kHz,
	// which is the setting that matches the attract music of the reference recording.
	// PJC_PSCALE overrides it at runtime for listening tests.
	MN1020819(config, m_soundcpu, 25_MHz_XTAL/2); // clock divisor unverified - NOTE: May have 64kB internal ROM
	// Calibrated against the ZN boards: their sound CPUs take ~1000 interrupts per
	// second, and the Power-JC engine drives its music from the same kind of tick
	// counter.  The unscaled divider runs the tick at ~2.6 kHz (music noticeably too
	// fast), while 540% lands the tick on ~999 Hz - the same rate the ZN boards run
	// at - and the attract music then lasts the same ~65 s as in the reference
	// recording.  PJC_PCT overrides it for listening tests.
	m_soundcpu->set_prescaler_percent(550);
	// The Power-JC board wires group 4 through group 2 and uses the 819's extra
	// 8-bit timer bank, so opt into the board quirks.  Other MN1020819DA boards
	// (e.g. Taito Type-Zero) run the plain device.
	m_soundcpu->set_power_jc_quirks(true);
	// Board ID straps.
	m_soundcpu->read_port<3>().set(FUNC(taitopjc_state::snd_port3_r));
	m_soundcpu->set_addrmap(AS_PROGRAM, &taitopjc_state::mn10200_map);

	// ZOOM ZSG-2 sample PCM, clocked from the 25MHz oscillator.
	ZSG2(config, m_zsg2, 25_MHz_XTAL);


	// The ZFX-2 runs firmware the game never uploads (only parameter blocks are sent
	// over the serial link), so the real chip cannot be emulated from the available
	// data - see the TODO block at the top of this file.
	//
	// TeknoTPJC, which is built on MAME's devices, does not emulate the ZFX-2 either
	// (its NOTICE lists only the MN10200, the ZSG-2 and the MB87078) and simply plays
	// the ZSG-2 out.  Do the same: mix all four ZSG-2 outputs - the two effect sends
	// and the dry stereo pair - straight onto the speakers.
	SPEAKER(config, "speaker", 2).front();
	m_zsg2->add_route(0, "speaker", 0.5, 0); // reverb send
	m_zsg2->add_route(1, "speaker", 0.5, 1); // chorus send
	m_zsg2->add_route(2, "speaker", 1.0, 0); // left direct
	m_zsg2->add_route(3, "speaker", 1.0, 1); // right direct

	config.set_maximum_quantum(attotime::from_hz(200000));

	NVRAM(config, "nvram", nvram_device::DEFAULT_ALL_0);

	screen_device &screen(SCREEN(config, "screen"));
	// V 0x1be / 0x14 / 0x194 / 0x1a8 / 0x1be
	// H 0x294 / 0x3f / 0x221 / 0x24c / 0x294
	// TODO: gets a 56.17 Hz with this, may be off by 1 or 2, verify on PCB, fix display ranges
	screen.set_raw(XTAL(16'537'000), 660, 0, 482, 446, 0, 384);
//  screen.set_visarea(0, 479, 0, 383);
	screen.set_screen_update(FUNC(taitopjc_state::screen_update));
	screen.set_palette(m_palette);

	PALETTE(config, m_palette).set_format(palette_device::xBGR_888, 32768);
	GFXDECODE(config, m_gfxdecode, m_palette, gfxdecode_device::empty);

	TC0780FPA(config, m_tc0780fpa);
}


void taitopjc_state::init_optiger()
{
#if 0
	// The original driver patched the I/O CPU's mailbox self-test out here
	// (0xfc0217 "jr C" -> two NOPs) because the 0x040040-0x0400ff window used to be
	// a stub that read back as zero.  That window is real dual-port RAM now, so the
	// walking-bit test at 0xfc1020/0xfc131c passes on its own and the patch is not
	// needed - keep it only as a reference, in case a future failure needs bisecting.
	uint8_t *rom = (uint8_t*)memregion("iocpu")->base();
	rom[BYTE_XOR_LE(0x217)] = 0x00;
	rom[BYTE_XOR_LE(0x218)] = 0x00;
#endif

#if 0
	uint32_t *mr = (uint32_t*)memregion("maindata")->base();
	//mr[(0x23a5c^4)/4] = 0x60000000;
	mr[((0x513b0-0x40000)^4)/4] = 0x38600001;
#endif
}


ROM_START( optiger ) // Ver 2.14 O
	ROM_REGION64_BE( 0x200000, "maindata", 0 )
	ROM_LOAD32_BYTE( "e63_33-1.ic23", 0x000000, 0x080000, CRC(5ab176e2) SHA1(a0a5b7c0e91928d0a49987f88f6ae647f5cb3e34) ) // PCB silkscreened IC23 (P-HH)
	ROM_LOAD32_BYTE( "e63_32-1.ic22", 0x000001, 0x080000, CRC(cca8bacc) SHA1(e5a081f5c12a52601745f5b67fe3412033581b00) ) // PCB silkscreened IC22 (P-HL)
	ROM_LOAD32_BYTE( "e63_31-1.ic8",  0x000002, 0x080000, CRC(ad69e649) SHA1(9fc853d2cb6e7cac87dc06bad91048f191b799c5) ) // PCB silkscreened IC8 (P-LH)
	ROM_LOAD32_BYTE( "e63_30-1.ic7",  0x000003, 0x080000, CRC(a6183479) SHA1(e556c3edf100342079e680ec666f018fca7a82b0) ) // PCB silkscreened IC7 (P-LL)

	ROM_REGION( 0x8000, "dsp", 0 ) // surface mounted on K11X0870A POWER JC MOTHER-G PCB
	ROM_LOAD( "tms320bc53pq80.ic8", 0x0000, 0x8000, CRC(4b8e7fd6) SHA1(07d354a2e4d7554e215fa8d91b5eeeaf573766b0) ) // decapped. TODO: believed to be a generic TI part, verify if it is and if dump is good, if so move in the CPU core

	ROM_REGION16_LE( 0x20000, "dspdata", 0 )
	ROM_LOAD16_BYTE( "taito_e63_04.ic29", 0x000000, 0x010000, CRC(eccae391) SHA1(e5293c16342cace54dc4b6dfb827558e18ac25a4) ) // PCB silkscreened IC29 (L)     AT27C512
	ROM_LOAD16_BYTE( "taito_e63_03.ic28", 0x000001, 0x010000, CRC(58fce52f) SHA1(1e3d9ee034b25e658ca45a8b900de2aa54b00135) ) // PCB silkscreened IC28 (H)     AT27C512

	ROM_REGION( 0x40000, "iocpu", 0 )
	ROM_LOAD16_BYTE( "e63_28-1.ic59", 0x000000, 0x020000, CRC(ef41ffaf) SHA1(419621f354f548180d37961b861304c469e43a65) ) // PCB silkscreened IC59 (0)     27C1001
	ROM_LOAD16_BYTE( "e63_27-1.ic58", 0x000001, 0x020000, CRC(facc17a7) SHA1(40d69840cfcfe5a509d69824c2994de56a3c6ece) ) // PCB silkscreened IC58 (1)     27C1001

	ROM_REGION( 0x80000, "mn10200", 0 ) // sound CPU to drive Zoom ZSG-2/Zoom ZFX-2
	ROM_LOAD16_BYTE( "e63_17-1.ic18", 0x000000, 0x040000, CRC(2a063d5b) SHA1(a2b2fe4d8bad1aef7d9dcc0be607cc4e5bc4f0eb) ) // PCB silkscreened IC18 (S-L)    27C2001
	ROM_LOAD16_BYTE( "e63_18-1.ic19", 0x000001, 0x040000, CRC(2f590881) SHA1(7fb827a676f45b24380558b0068b76cb858314f6) ) // PCB silkscreened IC19 (S-H)    27C2001

	ROM_REGION64_BE( 0x1000000, "maingfx", 0 ) // mask ROMs
	ROM_LOAD32_WORD_SWAP( "e63-21.ic24", 0x000000, 0x400000, CRC(c818b211) SHA1(dce07bfe71a9ba11c3f028a640226c6e59c6aece) ) // PCB silkscreened IC24 (C-H)    23C32000
	ROM_LOAD32_WORD_SWAP( "e63-15.ic9",  0x000002, 0x400000, CRC(4ec6a2d7) SHA1(2ee6270cff7ea2459121961a29d42e000cee2921) ) // PCB silkscreened IC9 (C-L)    23C32000
	ROM_LOAD32_WORD_SWAP( "e63-22.ic25", 0x800000, 0x400000, CRC(6d895eb6) SHA1(473795da42fd29841a926f18a93e5992f4feb27c) ) // PCB silkscreened IC25 (M-H)    23C32000
	ROM_LOAD32_WORD_SWAP( "e63-16.ic10", 0x800002, 0x400000, CRC(d39c1e34) SHA1(6db0ce2251841db3518a9bd9c4520c3c666d19a0) ) // PCB silkscreened IC10 (M-L)    23C32000

	ROM_REGION16_BE( 0x1000000, "poly", ROMREGION_ERASEFF ) // mask ROMs
	ROM_LOAD16_WORD_SWAP( "e63-09.ic3", 0x000000, 0x400000, CRC(c3e2b1e0) SHA1(ee71f3f59b46e26dbe2ff724da2c509267c8bf2f) ) // PCB silkscreened IC3 (POLY0)    23C32000
	ROM_LOAD16_WORD_SWAP( "e63-10.ic4", 0x400000, 0x400000, CRC(f4a56390) SHA1(fc3c51a7f4639479e66ad50dcc94255d94803c97) ) // PCB silkscreened IC4 (POLY1)    23C32000
	ROM_LOAD16_WORD_SWAP( "e63-11.ic5", 0x800000, 0x400000, CRC(2293d9f8) SHA1(16adaa0523168ee63a7a34b29622c623558fdd82) ) // PCB silkscreened IC5 (POLY2)    23C32000
	// IC6 (POLY3) is not populated

	ROM_REGION32_LE( 0x800000, "zsg2", 0 ) // mask ROMs - Zoom ZSG-2 samples (32-bit region, name must match the ZSG2 device tag)
	ROM_LOAD( "e63-23.ic36", 0x000000, 0x200000, CRC(d69e196e) SHA1(f738bb9e1330f6dabb5e0f0378a1a8eb48a4fa40) ) // PCB silkscreened IC36 (WD0)    23C16000
	ROM_LOAD( "e63-24.ic37", 0x200000, 0x200000, CRC(cd55f17b) SHA1(08f847ef2fd592dbaf63ef9e370cdf1f42012f74) ) // PCB silkscreened IC37 (WD1)    23C16000
	ROM_LOAD( "e63-25.ic38", 0x400000, 0x200000, CRC(bd35bdac) SHA1(5cde6c1a6b74659507b31fcb88257e65f230bfe2) ) // PCB silkscreened IC38 (WD2)    23C16000
	ROM_LOAD( "e63-26.ic39", 0x600000, 0x200000, CRC(346bd413) SHA1(0f6081d22db88eef08180278e7ae97283b5e8452) ) // PCB silkscreened IC39 (WD3)    23C16000

	ROM_REGION( 0x850, "plds", 0 )
	ROM_LOAD( "e63-01_palce16v8h-5-5.ic23",  0x000, 0x117, CRC(f114c13f) SHA1(ca9ec41d5c16347bdf107b340e6e1b9e6b7c74a9) )
	ROM_LOAD( "e63-02_palce22v10h-5-5.ic25", 0x117, 0x2dd, CRC(8418da84) SHA1(b235761f78ecb16d764fbefb00d04092d3a22ca9) )
	ROM_LOAD( "e63-05_palce16v8h-10-4.ic36", 0x3f4, 0x117, CRC(e27e9734) SHA1(77dadfbedb625b65617640bb73c59c9e5b0c927f) )
	ROM_LOAD( "e63-06_palce16v8h-10-4.ic41", 0x50b, 0x117, CRC(75184422) SHA1(d35e98e0278d713139eb1c833f41f57ed0dd3c9f) )
	ROM_LOAD( "e63-07_palce16v8h-10-4.ic43", 0x622, 0x117, CRC(eb77b03f) SHA1(567f92a4fd1fa919d5e9047ee15c058bf40855fb) )
	ROM_LOAD( "e63-08_palce16v8h-15-4.ic49", 0x739, 0x117, CRC(c305c56d) SHA1(49592fa43c548ac6b08951d03677a3f23e9c8de8) )
ROM_END

ROM_START( optigera ) // Ver 2.10 O
	ROM_REGION64_BE( 0x200000, "maindata", 0 )
	ROM_LOAD32_BYTE( "e63_33.ic23", 0x000000, 0x080000, CRC(414a7c77) SHA1(d4bbaa13244f1e5f4d418354f40303b9bcc00411) ) // PCB silkscreened IC23 (P-HH)
	ROM_LOAD32_BYTE( "e63_32.ic22", 0x000001, 0x080000, CRC(8fec33e8) SHA1(1eb0c5613937cd63dc2f54efa33c98920c55f251) ) // PCB silkscreened IC22 (P-HL)
	ROM_LOAD32_BYTE( "e63_31.ic8",  0x000002, 0x080000, CRC(672f9d4f) SHA1(7eb79963a5d4fb504ffbcf3f51c9bdf659ae053b) ) // PCB silkscreened IC8 (P-LH)
	ROM_LOAD32_BYTE( "e63_30.ic7",  0x000003, 0x080000, CRC(b5a63d08) SHA1(c12dd21008fb58179413934a5952ebd2d2040111) ) // PCB silkscreened IC7 (P-LL)

	ROM_REGION( 0x8000, "dsp", 0 ) // surface mounted on K11X0870A POWER JC MOTHER-G PCB
	ROM_LOAD( "tms320bc53pq80.ic8", 0x0000, 0x8000, CRC(4b8e7fd6) SHA1(07d354a2e4d7554e215fa8d91b5eeeaf573766b0) ) // decapped. TODO: believed to be a generic TI part, verify if it is and if dump is good, if so move in the CPU core

	ROM_REGION16_LE( 0x20000, "dspdata", 0 )
	ROM_LOAD16_BYTE( "taito_e63_04.ic29", 0x000000, 0x010000, CRC(eccae391) SHA1(e5293c16342cace54dc4b6dfb827558e18ac25a4) ) // PCB silkscreened IC29 (L)     AT27C512
	ROM_LOAD16_BYTE( "taito_e63_03.ic28", 0x000001, 0x010000, CRC(58fce52f) SHA1(1e3d9ee034b25e658ca45a8b900de2aa54b00135) ) // PCB silkscreened IC28 (H)     AT27C512

	ROM_REGION( 0x40000, "iocpu", 0 )
	ROM_LOAD16_BYTE( "e63_28.ic59", 0x000000, 0x020000, CRC(601dc916) SHA1(49b1629c4b5a5482c932ebd69b46b40489118012) ) // PCB silkscreened IC59 (0)     27C1001
	ROM_LOAD16_BYTE( "e63_27.ic58", 0x000001, 0x020000, CRC(930d899f) SHA1(fef194020c8c8b5906a6f2a954a1a0312d970f3d) ) // PCB silkscreened IC58 (1)     27C1001

	ROM_REGION( 0x80000, "mn10200", 0 ) // sound CPU to drive Zoom ZSG-2/Zoom ZFX-2
	ROM_LOAD16_BYTE( "e63_17.ic18", 0x000000, 0x040000, CRC(daac9e43) SHA1(9ef779a9a5e991ffcfcf30e94ef75329c1030fc2) ) // PCB silkscreened IC18 (S-L)    27C2001
	ROM_LOAD16_BYTE( "e63_18.ic19", 0x000001, 0x040000, CRC(69c97004) SHA1(65dc3dee0eb7faa1422c38947510abaeb23da7e3) ) // PCB silkscreened IC19 (S-H)    27C2001

	ROM_REGION64_BE( 0x1000000, "maingfx", 0 ) // mask ROMs
	ROM_LOAD32_WORD_SWAP( "e63-21.ic24", 0x000000, 0x400000, CRC(c818b211) SHA1(dce07bfe71a9ba11c3f028a640226c6e59c6aece) ) // PCB silkscreened IC24 (C-H)    23C32000
	ROM_LOAD32_WORD_SWAP( "e63-15.ic9",  0x000002, 0x400000, CRC(4ec6a2d7) SHA1(2ee6270cff7ea2459121961a29d42e000cee2921) ) // PCB silkscreened IC9 (C-L)    23C32000
	ROM_LOAD32_WORD_SWAP( "e63-22.ic25", 0x800000, 0x400000, CRC(6d895eb6) SHA1(473795da42fd29841a926f18a93e5992f4feb27c) ) // PCB silkscreened IC25 (M-H)    23C32000
	ROM_LOAD32_WORD_SWAP( "e63-16.ic10", 0x800002, 0x400000, CRC(d39c1e34) SHA1(6db0ce2251841db3518a9bd9c4520c3c666d19a0) ) // PCB silkscreened IC10 (M-L)    23C32000

	ROM_REGION16_BE( 0x1000000, "poly", ROMREGION_ERASEFF ) // mask ROMs
	ROM_LOAD16_WORD_SWAP( "e63-09.ic3", 0x000000, 0x400000, CRC(c3e2b1e0) SHA1(ee71f3f59b46e26dbe2ff724da2c509267c8bf2f) ) // PCB silkscreened IC3 (POLY0)    23C32000
	ROM_LOAD16_WORD_SWAP( "e63-10.ic4", 0x400000, 0x400000, CRC(f4a56390) SHA1(fc3c51a7f4639479e66ad50dcc94255d94803c97) ) // PCB silkscreened IC4 (POLY1)    23C32000
	ROM_LOAD16_WORD_SWAP( "e63-11.ic5", 0x800000, 0x400000, CRC(2293d9f8) SHA1(16adaa0523168ee63a7a34b29622c623558fdd82) ) // PCB silkscreened IC5 (POLY2)    23C32000
	// IC6 (POLY3) is not populated

	ROM_REGION32_LE( 0x800000, "zsg2", 0 ) // mask ROMs - Zoom ZSG-2 samples (32-bit region, name must match the ZSG2 device tag)
	ROM_LOAD( "e63-23.ic36", 0x000000, 0x200000, CRC(d69e196e) SHA1(f738bb9e1330f6dabb5e0f0378a1a8eb48a4fa40) ) // PCB silkscreened IC36 (WD0)    23C16000
	ROM_LOAD( "e63-24.ic37", 0x200000, 0x200000, CRC(cd55f17b) SHA1(08f847ef2fd592dbaf63ef9e370cdf1f42012f74) ) // PCB silkscreened IC37 (WD1)    23C16000
	ROM_LOAD( "e63-25.ic38", 0x400000, 0x200000, CRC(bd35bdac) SHA1(5cde6c1a6b74659507b31fcb88257e65f230bfe2) ) // PCB silkscreened IC38 (WD2)    23C16000
	ROM_LOAD( "e63-26.ic39", 0x600000, 0x200000, CRC(346bd413) SHA1(0f6081d22db88eef08180278e7ae97283b5e8452) ) // PCB silkscreened IC39 (WD3)    23C16000

	ROM_REGION( 0x850, "plds", 0 )
	ROM_LOAD( "e63-01_palce16v8h-5-5.ic23",  0x000, 0x117, CRC(f114c13f) SHA1(ca9ec41d5c16347bdf107b340e6e1b9e6b7c74a9) )
	ROM_LOAD( "e63-02_palce22v10h-5-5.ic25", 0x117, 0x2dd, CRC(8418da84) SHA1(b235761f78ecb16d764fbefb00d04092d3a22ca9) )
	ROM_LOAD( "e63-05_palce16v8h-10-4.ic36", 0x3f4, 0x117, CRC(e27e9734) SHA1(77dadfbedb625b65617640bb73c59c9e5b0c927f) )
	ROM_LOAD( "e63-06_palce16v8h-10-4.ic41", 0x50b, 0x117, CRC(75184422) SHA1(d35e98e0278d713139eb1c833f41f57ed0dd3c9f) )
	ROM_LOAD( "e63-07_palce16v8h-10-4.ic43", 0x622, 0x117, CRC(eb77b03f) SHA1(567f92a4fd1fa919d5e9047ee15c058bf40855fb) )
	ROM_LOAD( "e63-08_palce16v8h-15-4.ic49", 0x739, 0x117, CRC(c305c56d) SHA1(49592fa43c548ac6b08951d03677a3f23e9c8de8) )
ROM_END

ROM_START( optigerj ) // ver 2.09 J
	ROM_REGION64_BE( 0x200000, "maindata", 0 )
	ROM_LOAD32_BYTE( "e63_20.ic23", 0x000000, 0x080000, CRC(04b9820b) SHA1(7a5bd7fd7003948b57d862dc1ecd38ddd25a4ca2) ) // PCB silkscreened IC23 (P-HH)
	ROM_LOAD32_BYTE( "e63_19.ic22", 0x000001, 0x080000, CRC(a25ff024) SHA1(46e8023a028a384609177d00a47cdfdbbda100be) ) // PCB silkscreened IC22 (P-HL)
	ROM_LOAD32_BYTE( "e63_14.ic8",  0x000002, 0x080000, CRC(2e68fc12) SHA1(a8ee3e51ad8eb1477db8f67380261cee1dada104) ) // PCB silkscreened IC8 (P-LH)
	ROM_LOAD32_BYTE( "e63_13.ic7",  0x000003, 0x080000, CRC(d1c877e6) SHA1(05314984b7fbc21478b50f7b1281d1f3745496d4) ) // PCB silkscreened IC7 (P-LL)

	ROM_REGION( 0x8000, "dsp", 0 ) // surface mounted on K11X0870A POWER JC MOTHER-G PCB
	ROM_LOAD( "tms320bc53pq80.ic8", 0x0000, 0x8000, CRC(4b8e7fd6) SHA1(07d354a2e4d7554e215fa8d91b5eeeaf573766b0) ) // decapped. TODO: believed to be a generic TI part, verify if it is and if dump is good, if so move in the CPU core

	ROM_REGION16_LE( 0x20000, "dspdata", 0 )
	ROM_LOAD16_BYTE( "taito_e63_04.ic29", 0x000000, 0x010000, CRC(eccae391) SHA1(e5293c16342cace54dc4b6dfb827558e18ac25a4) ) // PCB silkscreened IC29 (L)     AT27C512
	ROM_LOAD16_BYTE( "taito_e63_03.ic28", 0x000001, 0x010000, CRC(58fce52f) SHA1(1e3d9ee034b25e658ca45a8b900de2aa54b00135) ) // PCB silkscreened IC28 (H)     AT27C512

	ROM_REGION( 0x40000, "iocpu", 0 )
	ROM_LOAD16_BYTE( "e63_28.ic59", 0x000000, 0x020000, CRC(601dc916) SHA1(49b1629c4b5a5482c932ebd69b46b40489118012) ) // PCB silkscreened IC59 (0)     27C1001
	ROM_LOAD16_BYTE( "e63_27.ic58", 0x000001, 0x020000, CRC(930d899f) SHA1(fef194020c8c8b5906a6f2a954a1a0312d970f3d) ) // PCB silkscreened IC58 (1)     27C1001

	ROM_REGION( 0x80000, "mn10200", 0 ) // sound CPU to drive Zoom ZSG-2/Zoom ZFX-2
	ROM_LOAD16_BYTE( "e63_17.ic18", 0x000000, 0x040000, CRC(daac9e43) SHA1(9ef779a9a5e991ffcfcf30e94ef75329c1030fc2) ) // PCB silkscreened IC18 (S-L)    27C2001
	ROM_LOAD16_BYTE( "e63_18.ic19", 0x000001, 0x040000, CRC(69c97004) SHA1(65dc3dee0eb7faa1422c38947510abaeb23da7e3) ) // PCB silkscreened IC19 (S-H)    27C2001

	ROM_REGION64_BE( 0x1000000, "maingfx", 0 ) // mask ROMs
	ROM_LOAD32_WORD_SWAP( "e63-21.ic24", 0x000000, 0x400000, CRC(c818b211) SHA1(dce07bfe71a9ba11c3f028a640226c6e59c6aece) ) // PCB silkscreened IC24 (C-H)    23C32000
	ROM_LOAD32_WORD_SWAP( "e63-15.ic9",  0x000002, 0x400000, CRC(4ec6a2d7) SHA1(2ee6270cff7ea2459121961a29d42e000cee2921) ) // PCB silkscreened IC9 (C-L)    23C32000
	ROM_LOAD32_WORD_SWAP( "e63-22.ic25", 0x800000, 0x400000, CRC(6d895eb6) SHA1(473795da42fd29841a926f18a93e5992f4feb27c) ) // PCB silkscreened IC25 (M-H)    23C32000
	ROM_LOAD32_WORD_SWAP( "e63-16.ic10", 0x800002, 0x400000, CRC(d39c1e34) SHA1(6db0ce2251841db3518a9bd9c4520c3c666d19a0) ) // PCB silkscreened IC10 (M-L)    23C32000

	ROM_REGION16_BE( 0x1000000, "poly", ROMREGION_ERASEFF ) // mask ROMs
	ROM_LOAD16_WORD_SWAP( "e63-09.ic3", 0x000000, 0x400000, CRC(c3e2b1e0) SHA1(ee71f3f59b46e26dbe2ff724da2c509267c8bf2f) ) // PCB silkscreened IC3 (POLY0)    23C32000
	ROM_LOAD16_WORD_SWAP( "e63-10.ic4", 0x400000, 0x400000, CRC(f4a56390) SHA1(fc3c51a7f4639479e66ad50dcc94255d94803c97) ) // PCB silkscreened IC4 (POLY1)    23C32000
	ROM_LOAD16_WORD_SWAP( "e63-11.ic5", 0x800000, 0x400000, CRC(2293d9f8) SHA1(16adaa0523168ee63a7a34b29622c623558fdd82) ) // PCB silkscreened IC5 (POLY2)    23C32000
	// IC6 (POLY3) is not populated

	ROM_REGION32_LE( 0x800000, "zsg2", 0 ) // mask ROMs - Zoom ZSG-2 samples (32-bit region, name must match the ZSG2 device tag)
	ROM_LOAD( "e63-23.ic36", 0x000000, 0x200000, CRC(d69e196e) SHA1(f738bb9e1330f6dabb5e0f0378a1a8eb48a4fa40) ) // PCB silkscreened IC36 (WD0)    23C16000
	ROM_LOAD( "e63-24.ic37", 0x200000, 0x200000, CRC(cd55f17b) SHA1(08f847ef2fd592dbaf63ef9e370cdf1f42012f74) ) // PCB silkscreened IC37 (WD1)    23C16000
	ROM_LOAD( "e63-25.ic38", 0x400000, 0x200000, CRC(bd35bdac) SHA1(5cde6c1a6b74659507b31fcb88257e65f230bfe2) ) // PCB silkscreened IC38 (WD2)    23C16000
	ROM_LOAD( "e63-26.ic39", 0x600000, 0x200000, CRC(346bd413) SHA1(0f6081d22db88eef08180278e7ae97283b5e8452) ) // PCB silkscreened IC39 (WD3)    23C16000

	ROM_REGION( 0x850, "plds", 0 )
	ROM_LOAD( "e63-01_palce16v8h-5-5.ic23",  0x000, 0x117, CRC(f114c13f) SHA1(ca9ec41d5c16347bdf107b340e6e1b9e6b7c74a9) )
	ROM_LOAD( "e63-02_palce22v10h-5-5.ic25", 0x117, 0x2dd, CRC(8418da84) SHA1(b235761f78ecb16d764fbefb00d04092d3a22ca9) )
	ROM_LOAD( "e63-05_palce16v8h-10-4.ic36", 0x3f4, 0x117, CRC(e27e9734) SHA1(77dadfbedb625b65617640bb73c59c9e5b0c927f) )
	ROM_LOAD( "e63-06_palce16v8h-10-4.ic41", 0x50b, 0x117, CRC(75184422) SHA1(d35e98e0278d713139eb1c833f41f57ed0dd3c9f) )
	ROM_LOAD( "e63-07_palce16v8h-10-4.ic43", 0x622, 0x117, CRC(eb77b03f) SHA1(567f92a4fd1fa919d5e9047ee15c058bf40855fb) )
	ROM_LOAD( "e63-08_palce16v8h-15-4.ic49", 0x739, 0x117, CRC(c305c56d) SHA1(49592fa43c548ac6b08951d03677a3f23e9c8de8) )
ROM_END

ROM_START( optigersm ) // Second Mission ver 2.02 J (build date shows 1999 but still (c) 1998)
	ROM_REGION64_BE( 0x200000, "maindata", 0 )
	ROM_LOAD32_BYTE( "e63_37.ic23", 0x000000, 0x080000, CRC(16692400) SHA1(0d9d6e90c763de66c2a99790cfe22e11c5d7ec42) ) // PCB silkscreened IC23 (P-HH)
	ROM_LOAD32_BYTE( "e63_36.ic22", 0x000001, 0x080000, CRC(99d6eed1) SHA1(96d7cff9fe5bedf79d3eaef0f19a4c69d0d233bf) ) // PCB silkscreened IC22 (P-HL)
	ROM_LOAD32_BYTE( "e63_35.ic8",  0x000002, 0x080000, CRC(85468b85) SHA1(7b24fb6eca29afbfe3ad0e6943145782d8d3b103) ) // PCB silkscreened IC8 (P-LH)
	ROM_LOAD32_BYTE( "e63_34.ic7",  0x000003, 0x080000, CRC(ba3cc7d4) SHA1(91fb1366c2d225483621beabcdc7ee2f337c8fc0) ) // PCB silkscreened IC7 (P-LL)

	ROM_REGION( 0x8000, "dsp", 0 ) // surface mounted on K11X0870A POWER JC MOTHER-G PCB
	ROM_LOAD( "tms320bc53pq80.ic8", 0x0000, 0x8000, CRC(4b8e7fd6) SHA1(07d354a2e4d7554e215fa8d91b5eeeaf573766b0) ) // decapped. TODO: believed to be a generic TI part, verify if it is and if dump is good, if so move in the CPU core

	ROM_REGION16_LE( 0x20000, "dspdata", 0 )
	ROM_LOAD16_BYTE( "taito_e63_04.ic29", 0x000000, 0x010000, CRC(eccae391) SHA1(e5293c16342cace54dc4b6dfb827558e18ac25a4) ) // PCB silkscreened IC29 (L)     AT27C512
	ROM_LOAD16_BYTE( "taito_e63_03.ic28", 0x000001, 0x010000, CRC(58fce52f) SHA1(1e3d9ee034b25e658ca45a8b900de2aa54b00135) ) // PCB silkscreened IC28 (H)     AT27C512

	ROM_REGION( 0x40000, "iocpu", 0 )
	ROM_LOAD16_BYTE( "e63_28-1.ic59", 0x000000, 0x020000, CRC(ef41ffaf) SHA1(419621f354f548180d37961b861304c469e43a65) ) // PCB silkscreened IC59 (0)     27C1001
	ROM_LOAD16_BYTE( "e63_27-1.ic58", 0x000001, 0x020000, CRC(facc17a7) SHA1(40d69840cfcfe5a509d69824c2994de56a3c6ece) ) // PCB silkscreened IC58 (1)     27C1001

	ROM_REGION( 0x80000, "mn10200", 0 ) // sound CPU to drive Zoom ZSG-2/Zoom ZFX-2
	ROM_LOAD16_BYTE( "e63_17-1.ic18", 0x000000, 0x040000, CRC(2a063d5b) SHA1(a2b2fe4d8bad1aef7d9dcc0be607cc4e5bc4f0eb) ) // PCB silkscreened IC18 (S-L)    27C2001
	ROM_LOAD16_BYTE( "e63_18-1.ic19", 0x000001, 0x040000, CRC(2f590881) SHA1(7fb827a676f45b24380558b0068b76cb858314f6) ) // PCB silkscreened IC19 (S-H)    27C2001

	ROM_REGION64_BE( 0x1000000, "maingfx", 0 ) // mask ROMs
	ROM_LOAD32_WORD_SWAP( "e63-21.ic24", 0x000000, 0x400000, CRC(c818b211) SHA1(dce07bfe71a9ba11c3f028a640226c6e59c6aece) ) // PCB silkscreened IC24 (C-H)    23C32000
	ROM_LOAD32_WORD_SWAP( "e63-15.ic9",  0x000002, 0x400000, CRC(4ec6a2d7) SHA1(2ee6270cff7ea2459121961a29d42e000cee2921) ) // PCB silkscreened IC9 (C-L)    23C32000
	ROM_LOAD32_WORD_SWAP( "e63-22.ic25", 0x800000, 0x400000, CRC(6d895eb6) SHA1(473795da42fd29841a926f18a93e5992f4feb27c) ) // PCB silkscreened IC25 (M-H)    23C32000
	ROM_LOAD32_WORD_SWAP( "e63-16.ic10", 0x800002, 0x400000, CRC(d39c1e34) SHA1(6db0ce2251841db3518a9bd9c4520c3c666d19a0) ) // PCB silkscreened IC10 (M-L)    23C32000

	ROM_REGION16_BE( 0x1000000, "poly", ROMREGION_ERASEFF ) // mask ROMs
	ROM_LOAD16_WORD_SWAP( "e63-09.ic3", 0x000000, 0x400000, CRC(c3e2b1e0) SHA1(ee71f3f59b46e26dbe2ff724da2c509267c8bf2f) ) // PCB silkscreened IC3 (POLY0)    23C32000
	ROM_LOAD16_WORD_SWAP( "e63-10.ic4", 0x400000, 0x400000, CRC(f4a56390) SHA1(fc3c51a7f4639479e66ad50dcc94255d94803c97) ) // PCB silkscreened IC4 (POLY1)    23C32000
	ROM_LOAD16_WORD_SWAP( "e63-11.ic5", 0x800000, 0x400000, CRC(2293d9f8) SHA1(16adaa0523168ee63a7a34b29622c623558fdd82) ) // PCB silkscreened IC5 (POLY2)    23C32000
	// IC6 (POLY3) is not populated

	ROM_REGION32_LE( 0x800000, "zsg2", 0 ) // mask ROMs - Zoom ZSG-2 samples (32-bit region, name must match the ZSG2 device tag)
	ROM_LOAD( "e63-23.ic36", 0x000000, 0x200000, CRC(d69e196e) SHA1(f738bb9e1330f6dabb5e0f0378a1a8eb48a4fa40) ) // PCB silkscreened IC36 (WD0)    23C16000
	ROM_LOAD( "e63-24.ic37", 0x200000, 0x200000, CRC(cd55f17b) SHA1(08f847ef2fd592dbaf63ef9e370cdf1f42012f74) ) // PCB silkscreened IC37 (WD1)    23C16000
	ROM_LOAD( "e63-25.ic38", 0x400000, 0x200000, CRC(bd35bdac) SHA1(5cde6c1a6b74659507b31fcb88257e65f230bfe2) ) // PCB silkscreened IC38 (WD2)    23C16000
	ROM_LOAD( "e63-26.ic39", 0x600000, 0x200000, CRC(346bd413) SHA1(0f6081d22db88eef08180278e7ae97283b5e8452) ) // PCB silkscreened IC39 (WD3)    23C16000

	ROM_REGION( 0x850, "plds", 0 )
	ROM_LOAD( "e63-01_palce16v8h-5-5.ic23",  0x000, 0x117, CRC(f114c13f) SHA1(ca9ec41d5c16347bdf107b340e6e1b9e6b7c74a9) )
	ROM_LOAD( "e63-02_palce22v10h-5-5.ic25", 0x117, 0x2dd, CRC(8418da84) SHA1(b235761f78ecb16d764fbefb00d04092d3a22ca9) )
	ROM_LOAD( "e63-05_palce16v8h-10-4.ic36", 0x3f4, 0x117, CRC(e27e9734) SHA1(77dadfbedb625b65617640bb73c59c9e5b0c927f) )
	ROM_LOAD( "e63-06_palce16v8h-10-4.ic41", 0x50b, 0x117, CRC(75184422) SHA1(d35e98e0278d713139eb1c833f41f57ed0dd3c9f) )
	ROM_LOAD( "e63-07_palce16v8h-10-4.ic43", 0x622, 0x117, CRC(eb77b03f) SHA1(567f92a4fd1fa919d5e9047ee15c058bf40855fb) )
	ROM_LOAD( "e63-08_palce16v8h-15-4.ic49", 0x739, 0x117, CRC(c305c56d) SHA1(49592fa43c548ac6b08951d03677a3f23e9c8de8) )
ROM_END

} // anonymous namespace


GAME( 1998, optiger,   0,       taitopjc, taitopjc, taitopjc_state, init_optiger, ROT0, "Taito", "Operation Tiger (Ver 2.14 O)",                MACHINE_NOT_WORKING | MACHINE_IMPERFECT_GRAPHICS | MACHINE_IMPERFECT_TIMING | MACHINE_IMPERFECT_SOUND ) // Operation Tiger Ver 2.14 O, Oct  5 1998  13:58:13
GAME( 1998, optigera,  optiger, taitopjc, taitopjc, taitopjc_state, init_optiger, ROT0, "Taito", "Operation Tiger (Ver 2.10 O)",                MACHINE_NOT_WORKING | MACHINE_IMPERFECT_GRAPHICS | MACHINE_IMPERFECT_TIMING | MACHINE_IMPERFECT_SOUND ) // Operation Tiger Ver 2.10 O, Sep 19 1998  14:06:18
GAME( 1998, optigerj,  optiger, taitopjc, taitopjc, taitopjc_state, init_optiger, ROT0, "Taito", "Operation Tiger (Ver 2.09 J)",                MACHINE_NOT_WORKING | MACHINE_IMPERFECT_GRAPHICS | MACHINE_IMPERFECT_TIMING | MACHINE_IMPERFECT_SOUND ) // Operation Tiger Ver 2.09 J, Sep 12 1998  19:28:59
GAME( 1998, optigersm, 0,       taitopjc, taitopjc, taitopjc_state, init_optiger, ROT0, "Taito", "Operation Tiger Second Mission (Ver 2.02 J)", MACHINE_NOT_WORKING | MACHINE_IMPERFECT_GRAPHICS | MACHINE_IMPERFECT_TIMING | MACHINE_IMPERFECT_SOUND ) // Operation Tiger Ver 2.02 J, Second Mission, Feb 16 1999  14:23:36
