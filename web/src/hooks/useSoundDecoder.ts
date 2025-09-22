import { useCallback, useEffect, useRef, useState } from 'react';
import ggwaveFactory from 'ggwave';
import type { GgwaveModule } from 'ggwave';

export type ListenStatus = 'idle' | 'listening' | 'decoding' | 'error';

export interface ListenOptions {
  durationMs?: number;
  onLevel?: (level: number) => void;
}

export interface UseSoundDecoderResult {
  ready: boolean;
  error: string | null;
  listen: (options: ListenOptions) => Promise<Uint8Array | null>;
  release: () => void;
  status: ListenStatus;
}

const clamp = (value: number, min: number, max: number) => Math.min(Math.max(value, min), max);

const convertToInt16 = (chunks: Float32Array[]): Int16Array => {
  const total = chunks.reduce((acc, chunk) => acc + chunk.length, 0);
  const output = new Int16Array(total);
  let offset = 0;
  for (const chunk of chunks) {
    for (let i = 0; i < chunk.length; i += 1) {
      const s = clamp(chunk[i], -1, 1);
      output[offset + i] = s < 0 ? Math.round(s * 0x8000) : Math.round(s * 0x7fff);
    }
    offset += chunk.length;
  }
  return output;
};

const decodeWithGgwave = (
  module: GgwaveModule,
  samples: Int16Array,
  sampleRate: number,
): Uint8Array | null => {
  const params = module.getDefaultParameters();
  params.payloadLength = 36;
  params.sampleRateInp = sampleRate;
  params.sampleRateOut = sampleRate;
  params.sampleRate = sampleRate;
  params.sampleFormatInp = module.SampleFormat.GGWAVE_SAMPLE_FORMAT_I16;
  params.sampleFormatOut = module.SampleFormat.GGWAVE_SAMPLE_FORMAT_I16;
  params.operatingMode = module.GGWAVE_OPERATING_MODE_RX;

  const instance = module.init(params);
  try {
    const view = new Uint8Array(samples.buffer, samples.byteOffset, samples.byteLength);
    const decoded = module.decode(instance, view);
    const result = new Uint8Array(decoded);
    return result.length > 0 ? new Uint8Array(result) : null;
  } finally {
    module.free(instance);
  }
};

export const useSoundDecoder = (): UseSoundDecoderResult => {
  const [module, setModule] = useState<GgwaveModule | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [status, setStatus] = useState<ListenStatus>('idle');
  const abortRef = useRef<AbortController | null>(null);

  useEffect(() => {
    let cancelled = false;
    ggwaveFactory()
      .then((ggwave) => {
        if (cancelled) return;
        ggwave.disableLog?.();
        setModule(ggwave);
      })
      .catch((err: unknown) => {
        if (cancelled) return;
        setError(err instanceof Error ? err.message : String(err));
      });
    return () => {
      cancelled = true;
    };
  }, []);

  const release = useCallback(() => {
    abortRef.current?.abort();
    abortRef.current = null;
    setStatus('idle');
  }, []);

  const listen = useCallback(
    async ({ durationMs = 6000, onLevel }: ListenOptions): Promise<Uint8Array | null> => {
      if (!module) {
        throw new Error('Движок GGWave еще не загружен');
      }

      if (!navigator.mediaDevices?.getUserMedia) {
        throw new Error('Микрофон недоступен в этом браузере');
      }

      setStatus('listening');
      const controller = new AbortController();
      abortRef.current = controller;

      const audioContext = new AudioContext();
      await audioContext.resume();
      const stream = await navigator.mediaDevices.getUserMedia({
        audio: {
          echoCancellation: false,
          noiseSuppression: false,
          autoGainControl: false,
          channelCount: 1,
        },
      });

      const source = audioContext.createMediaStreamSource(stream);
      const processor = audioContext.createScriptProcessor(4096, 1, 1);
      const silenceGain = audioContext.createGain();
      silenceGain.gain.value = 0;

      source.connect(processor);
      processor.connect(silenceGain);
      silenceGain.connect(audioContext.destination);

      const chunks: Float32Array[] = [];

      processor.onaudioprocess = (event) => {
        if (controller.signal.aborted) {
          return;
        }
        const input = event.inputBuffer.getChannelData(0);
        chunks.push(new Float32Array(input));
        if (onLevel) {
          let sum = 0;
          for (let i = 0; i < input.length; i += 1) {
            sum += input[i] * input[i];
          }
          const rms = Math.sqrt(sum / input.length);
          onLevel(clamp(rms * 4, 0, 1));
        }
      };

      const waitForDuration = (ms: number, signal: AbortSignal) => new Promise<void>((resolve) => {
        const timer = setTimeout(() => {
          signal.removeEventListener('abort', onAbort);
          resolve();
        }, ms);
        const onAbort = () => {
          clearTimeout(timer);
          signal.removeEventListener('abort', onAbort);
          resolve();
        };
        signal.addEventListener('abort', onAbort, { once: true });
      });

      let sampleRate = audioContext.sampleRate;
      try {
        await waitForDuration(durationMs, controller.signal);
        sampleRate = audioContext.sampleRate;
      } finally {
        processor.disconnect();
        silenceGain.disconnect();
        source.disconnect();
        stream.getTracks().forEach((track) => track.stop());
        await audioContext.close();
        abortRef.current = null;
      }

      if (controller.signal.aborted) {
        setStatus('idle');
        return null;
      }

      if (chunks.length === 0) {
        setStatus('error');
        throw new Error('Нет данных с микрофона');
      }

      setStatus('decoding');
      try {
        const samples = convertToInt16(chunks);
        const decoded = decodeWithGgwave(module, samples, sampleRate);
        setStatus(decoded ? 'idle' : 'error');
        return decoded;
      } catch (err) {
        setStatus('error');
        throw err;
      }
    },
    [module],
  );

  return {
    ready: Boolean(module) && !error,
    error,
    listen,
    release,
    status,
  };
};
