#!/usr/bin/env python3

#
# This file is part of LiteX-Boards.
#
# Copyright (c) 2021 Kazumoto Kojima <kkojima@rr.iij4u.or.jp>
# SPDX-License-Identifier: BSD-2-Clause

from migen import *

from litex.gen import *

from litex.build.io import DDROutput

from litex_boards.platforms import colorlight_i5

from litex.soc.cores.clock import *
from litex.soc.integration.soc_core import *
from litex.soc.integration.builder import *
from litex.soc.cores.video import VideoHDMIPHY
from litex.soc.cores.led import LedChaser
from litex.build.generic_platform import Subsignal, Pins, IOStandard

from litex.soc.interconnect.csr import *
from litex.soc.cores.gpio import GPIOOut

from litedram.modules import M12L64322A
from litedram.phy import GENSDRPHY, HalfRateGENSDRPHY

# CRG ----------------------------------------------------------------------------------------------

class _CRG(LiteXModule):
    def __init__(self, platform, sys_clk_freq, use_internal_osc=False, with_usb_pll=False, with_video_pll=False, sdram_rate="1:1"):
        self.rst    = Signal()
        self.cd_sys = ClockDomain()
        if sdram_rate == "1:2":
            self.cd_sys2x    = ClockDomain()
            self.cd_sys2x_ps = ClockDomain()
        else:
            self.cd_sys_ps = ClockDomain()

        # # #

        # Clk / Rst
        if not use_internal_osc:
            clk = platform.request("clk25")
            clk_freq = 25e6
        else:
            clk = Signal()
            div = 5
            self.specials += Instance("OSCG",
                p_DIV = div,
                o_OSC = clk
            )
            clk_freq = 310e6/div

        rst_n = platform.request("cpu_reset_n")

        # PLL
        self.pll = pll = ECP5PLL()
        self.comb += pll.reset.eq(~rst_n | self.rst)
        pll.register_clkin(clk, clk_freq)
        pll.create_clkout(self.cd_sys,    sys_clk_freq)
        if sdram_rate == "1:2":
            pll.create_clkout(self.cd_sys2x,    2*sys_clk_freq)
            pll.create_clkout(self.cd_sys2x_ps, 2*sys_clk_freq, phase=180) # Idealy 90° but needs to be increased.
        else:
           pll.create_clkout(self.cd_sys_ps, sys_clk_freq, phase=180) # Idealy 90° but needs to be increased.

        # USB PLL
        if with_usb_pll:
            self.usb_pll = usb_pll = ECP5PLL()
            self.comb += usb_pll.reset.eq(~rst_n | self.rst)
            usb_pll.register_clkin(clk, clk_freq)
            self.cd_usb_12 = ClockDomain()
            self.cd_usb_48 = ClockDomain()
            usb_pll.create_clkout(self.cd_usb_12, 12e6, margin=0)
            usb_pll.create_clkout(self.cd_usb_48, 48e6, margin=0)

        # Video PLL
        if with_video_pll:
            self.video_pll = video_pll = ECP5PLL()
            self.comb += video_pll.reset.eq(~rst_n | self.rst)
            video_pll.register_clkin(clk, clk_freq)
            self.cd_hdmi   = ClockDomain()
            self.cd_hdmi5x = ClockDomain()
            video_pll.create_clkout(self.cd_hdmi,    40e6, margin=0)
            video_pll.create_clkout(self.cd_hdmi5x, 200e6, margin=0)

        # SDRAM clock
        sdram_clk = ClockSignal("sys2x_ps" if sdram_rate == "1:2" else "sys_ps")
        self.specials += DDROutput(1, 0, platform.request("sdram_clock"), sdram_clk)

# BaseSoC ------------------------------------------------------------------------------------------

class BaseSoC(SoCCore):
    def __init__(self, board="i5", revision="7.0", toolchain="trellis", sys_clk_freq=60e6,
        with_led_chaser        = True,
        use_internal_osc       = False,
        sdram_rate             = "1:1",
        with_video_terminal    = False,
        with_video_framebuffer = False,
        **kwargs):
        board = board.lower()
        assert board in ["i5", "i9"]
        platform = colorlight_i5.Platform(board=board, revision=revision, toolchain=toolchain)

        # CRG --------------------------------------------------------------------------------------
        with_usb_pll   = kwargs.get("uart_name", None) == "usb_acm"
        with_video_pll = with_video_terminal or with_video_framebuffer
        self.crg = _CRG(platform, sys_clk_freq,
            use_internal_osc = use_internal_osc,
            with_usb_pll     = with_usb_pll,
            with_video_pll   = with_video_pll,
            sdram_rate       = sdram_rate
        )

        # SoCCore ----------------------------------------------------------------------------------
        SoCCore.__init__(self, platform, int(sys_clk_freq), ident = "LiteX SoC on Colorlight " + board.upper(), **kwargs)

        # Leds -------------------------------------------------------------------------------------
        # LED on-board desabilitado para usar GPIO customizada de 8 LEDs
        # if with_led_chaser:
        #     ledn = platform.request_all("user_led_n")
        #     self.leds = LedChaser(pads=ledn, sys_clk_freq=sys_clk_freq)

    # SPI Flash --------------------------------------------------------------------------------
        if board == "i5":
            from litespi.modules import GD25Q16 as SpiFlashModule
        if board == "i9":
            from litespi.modules import W25Q64 as SpiFlashModule

        from litespi.opcodes import SpiNorFlashOpCodes as Codes
        self.add_spi_flash(mode="1x", module=SpiFlashModule(Codes.READ_1_1_1))

        # SDR SDRAM --------------------------------------------------------------------------------
        if not self.integrated_main_ram_size:
            sdrphy_cls = HalfRateGENSDRPHY if sdram_rate == "1:2" else GENSDRPHY
            self.sdrphy = sdrphy_cls(platform.request("sdram"))
            self.add_sdram("sdram",
                phy           = self.sdrphy,
                module        = M12L64322A(sys_clk_freq, sdram_rate),
                l2_cache_size = kwargs.get("l2_size", 8192)
            )

        # Configuração dos pinos para 8 LEDs externos (placa de expansão) --------------------------
        # Conector: CN2 (conector IDC 14 pinos da placa HwIT)
        # Pinos físicos conforme documentação CN2: P17 P18 N18 L20 L18 G20 M18 N17
        # Mapeamento: bit0(L1)->P17, bit1(L2)->P18, ..., bit7(L8)->N17
        # Nota: Conector IDC 2x7 (14 pinos), pino 1 = faixa vermelha do flat cable
        leds_pads = [
            ("leds_ext", 0, 
                Pins("P17 P18 N18 L20 L18 G20 M18 N17"),
                IOStandard("LVCMOS33")
            )
        ]

        platform.add_extension(leds_pads)
        
        # Substitui o controle padrão de LEDs por GPIO de 8 bits para placa de expansão
        self.submodules.leds = GPIOOut(platform.request("leds_ext"))
        self.add_csr("leds")
   

# Build --------------------------------------------------------------------------------------------

def main():
    from litex.build.parser import LiteXArgumentParser
    parser = LiteXArgumentParser(platform=colorlight_i5.Platform, description="LiteX SoC on Colorlight I5.")
    parser.add_target_argument("--board",            default="i5",             help="Board type (i5).")
    parser.add_target_argument("--revision",         default="7.0",            help="Board revision (7.0).")
    parser.add_target_argument("--sys-clk-freq",     default=60e6, type=float, help="System clock frequency.")
    ethopts = parser.target_group.add_mutually_exclusive_group()
    ethopts.add_argument("--with-ethernet",   action="store_true",      help="Enable Ethernet support.")
    ethopts.add_argument("--with-etherbone",  action="store_true",      help="Enable Etherbone support.")
    parser.add_target_argument("--remote-ip", default="192.168.1.100",  help="Remote IP address of TFTP server.")
    parser.add_target_argument("--local-ip",  default="192.168.1.50",   help="Local IP address.")
    sdopts = parser.target_group.add_mutually_exclusive_group()
    sdopts.add_argument("--with-spi-sdcard",  action="store_true", help="Enable SPI-mode SDCard support.")
    sdopts.add_argument("--with-sdcard",      action="store_true", help="Enable SDCard support.")
    parser.add_target_argument("--eth-phy",          default=0, type=int, help="Ethernet PHY (0 or 1).")
    parser.add_target_argument("--use-internal-osc", action="store_true", help="Use internal oscillator.")
    parser.add_target_argument("--sdram-rate",       default="1:1",       help="SDRAM Rate (1:1 Full Rate or 1:2 Half Rate).")
    viopts = parser.target_group.add_mutually_exclusive_group()
    viopts.add_argument("--with-video-terminal",    action="store_true", help="Enable Video Terminal (HDMI).")
    viopts.add_argument("--with-video-framebuffer", action="store_true", help="Enable Video Framebuffer (HDMI).")
    # Recursos do projeto LoRa/AHT10
    parser.add_target_argument("--with-lora",     action="store_true", help="Habilita SPI para módulo LoRa (RFM96).")
    parser.add_target_argument("--with-aht10",    action="store_true", help="Habilita I2C para sensor AHT10.")
    parser.add_target_argument("--use-example-pins", action="store_true", help="Carrega arquivo de pinos de exemplo (edite pins_colorlight_i9_ext.py).")
    
    args = parser.parse_args()

    soc = BaseSoC(board=args.board, revision=args.revision,
        toolchain              = args.toolchain,
        sys_clk_freq           = args.sys_clk_freq,
        with_ethernet          = args.with_ethernet,
        sdram_rate             = args.sdram_rate,
        with_etherbone         = args.with_etherbone,
        local_ip               = args.local_ip,
        remote_ip              = args.remote_ip,
        eth_phy                = args.eth_phy,
        use_internal_osc       = args.use_internal_osc,
        with_video_terminal    = args.with_video_terminal,
        with_video_framebuffer = args.with_video_framebuffer,
        with_lora_spi          = args.with_lora,
        with_i2c_aht10         = args.with_aht10,
        use_example_pins       = args.use_example_pins,
        **parser.soc_argdict
    )
    soc.platform.add_extension(colorlight_i5._sdcard_pmod_io)
    if args.with_spi_sdcard:
        soc.add_spi_sdcard()
    if args.with_sdcard:
        soc.add_sdcard()

    builder = Builder(soc, **parser.builder_argdict)
    if args.build:
        builder.build(**parser.toolchain_argdict)

    if args.load:
        prog = soc.platform.create_programmer()
        prog.load_bitstream(builder.get_bitstream_filename(mode="sram"))

if __name__ == "__main__":
    main()