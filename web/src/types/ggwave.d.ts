declare module 'ggwave' {
  export interface GgwaveParameters {
    payloadLength: number;
    sampleRateInp: number;
    sampleRateOut: number;
    sampleRate: number;
    samplesPerFrame: number;
    soundMarkerThreshold: number;
    sampleFormatInp: number;
    sampleFormatOut: number;
    operatingMode: number;
  }

  export interface GgwaveModule {
    SampleFormat: {
      GGWAVE_SAMPLE_FORMAT_UNDEFINED: number;
      GGWAVE_SAMPLE_FORMAT_U8: number;
      GGWAVE_SAMPLE_FORMAT_I8: number;
      GGWAVE_SAMPLE_FORMAT_U16: number;
      GGWAVE_SAMPLE_FORMAT_I16: number;
      GGWAVE_SAMPLE_FORMAT_F32: number;
    };
    ProtocolId: Record<string, number>;
    GGWAVE_OPERATING_MODE_RX: number;
    GGWAVE_OPERATING_MODE_TX: number;
    GGWAVE_OPERATING_MODE_RX_AND_TX: number;
    GGWAVE_OPERATING_MODE_TX_ONLY_TONES: number;
    GGWAVE_OPERATING_MODE_USE_DSS: number;
    getDefaultParameters(): GgwaveParameters;
    init(parameters: GgwaveParameters): number;
    free(instance: number): void;
    decode(instance: number, data: Uint8Array | Int8Array | Int16Array | Uint16Array | Float32Array): Uint8Array;
    encode(
      instance: number,
      payload: string,
      protocolId: number,
      volume: number,
    ): Uint8Array;
    disableLog?: () => void;
  }

  export default function ggwaveFactory(): Promise<GgwaveModule>;
}
