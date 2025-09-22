import { useEffect, useMemo, useState } from 'react';
import { MicLevelMeter } from './components/MicLevelMeter';
import { AdminPanel } from './components/AdminPanel';
import { useSoundDecoder } from './hooks/useSoundDecoder';
import type { ListenStatus } from './hooks/useSoundDecoder';
import { getSession, requestChallenge, verifyToken } from './api/aat';
import type { SessionResponse } from './api/aat';
import { bytesToBase64 } from './utils/base64';
import { formatChallengeDecimal, formatChallengeHex } from './utils/challenge';
import type { ChallengeResponse } from './api/aat';
import './App.css';

const decodeStatusLabel = (status: ListenStatus) => {
  switch (status) {
    case 'idle':
      return 'Готов к прослушиванию';
    case 'listening':
      return 'Идет прослушивание микрофона';
    case 'decoding':
      return 'Декодируем токен...';
    case 'error':
      return 'Ошибка прослушивания';
    default:
      return '';
  }
};

const errorMessages: Record<string, string> = {
  bad_length: 'Получен поврежденный токен. Попробуйте еще раз.',
  bad_version: 'Неподдерживаемая версия токена.',
  unknown_kid: 'Неизвестный идентификатор устройства. Обновите приложение.',
  tag: 'Не удалось подтвердить подпись токена.',
  tslot_window: 'Время передачи истекло. Сформируйте новый токен.',
  bind32_mismatch: 'Токен не соответствует текущему challenge.',
  replay: 'Токен уже использовался. Сформируйте новый.',
  scope_mismatch: 'Токен предназначен для другого сервиса.',
  challenge_expired: 'Challenge устарел. Повторите попытку.',
};

type FlowStatus =
  | 'idle'
  | 'preparing'
  | 'listening'
  | 'decoding'
  | 'verifying'
  | 'success'
  | 'error';

const flowStatusMessage: Record<FlowStatus, string> = {
  idle: 'Нажмите кнопку, когда будете готовы воспроизвести звук с телефона.',
  preparing: 'Готовим challenge и проверяем микрофон...',
  listening: 'Слушаем микрофон (≈6 секунд). Держите телефон в 10–30 см от ноутбука.',
  decoding: 'Пытаемся декодировать токен...',
  verifying: 'Проверяем токен на сервере...',
  success: 'Токен принят! Вы будете перенаправлены в личный кабинет.',
  error: 'Что-то пошло не так. Проверьте окружение и попробуйте еще раз.',
};

const useCountdown = (challenge: ChallengeResponse | null) => {
  const [seconds, setSeconds] = useState<number>(0);

  useEffect(() => {
    if (!challenge) {
      setSeconds(0);
      return;
    }
    setSeconds(challenge.ttl);
    const timer = setInterval(() => {
      setSeconds((prev) => (prev > 0 ? prev - 1 : 0));
    }, 1000);
    return () => clearInterval(timer);
  }, [challenge?.id]);

  return seconds;
};

function App() {
  const decoder = useSoundDecoder();
  const [flowStatus, setFlowStatus] = useState<FlowStatus>('idle');
  const [level, setLevel] = useState(0);
  const [error, setError] = useState<string | null>(null);
  const [challenge, setChallenge] = useState<ChallengeResponse | null>(null);
  const [session, setSession] = useState<SessionResponse | null>(null);
  const [lastTokenBase64, setLastTokenBase64] = useState<string | null>(null);
  const [isBusy, setIsBusy] = useState(false);
  const [activeTab, setActiveTab] = useState<'login' | 'admin'>('login');

  const countdown = useCountdown(challenge);

  useEffect(() => {
    getSession()
      .then((res) => setSession(res))
      .catch(() => undefined);
  }, []);

  const statusMessage = useMemo(() => {
    if (error) {
      return error;
    }
    if (flowStatus === 'listening') {
      return `${flowStatusMessage[flowStatus]} (остаток: ${Math.max(countdown, 0)} сек)`;
    }
    return flowStatusMessage[flowStatus];
  }, [flowStatus, countdown, error]);

  const handleStart = async () => {
    if (!decoder.ready) {
      setError(decoder.error ?? 'Модуль GGWave загружается. Попробуйте через секунду.');
      setFlowStatus('error');
      return;
    }
    setIsBusy(true);
    setError(null);
    setLastTokenBase64(null);
    setFlowStatus('preparing');
    try {
      const challengeResponse = await requestChallenge();
      setChallenge(challengeResponse);
      setFlowStatus('listening');
      const tokenBytes = await decoder.listen({
        durationMs: 6000,
        onLevel: setLevel,
      });
      if (!tokenBytes) {
        setFlowStatus('idle');
        setLevel(0);
        return;
      }
      setFlowStatus('decoding');
      const tokenB64 = bytesToBase64(tokenBytes);
      setLastTokenBase64(tokenB64);
      setFlowStatus('verifying');
      const verify = await verifyToken(tokenB64, challengeResponse.id);
      if (verify.ok) {
        setFlowStatus('success');
        setSession({ ok: true, userId: verify.userId });
      } else {
        setFlowStatus('error');
        setError(errorMessages[verify.err ?? 'bad_length'] ?? 'Ошибка проверки токена.');
      }
    } catch (err) {
      setFlowStatus('error');
      setError(err instanceof Error ? err.message : String(err));
    } finally {
      setIsBusy(false);
      setLevel(0);
    }
  };

  const activeChallenge = challenge && flowStatus !== 'idle' ? challenge : null;

  const renderLogin = () => (
    <main className="app__card">
      <section className="app__left" aria-live="polite">
          <div className="app__title">
            <span className="app__badge">EchoPass / AAT</span>
          </div>
          <h1 className="app__headline">Вход по звуку</h1>
          <p className="app__subtitle">
            Сгенерируйте токен в мобильном приложении EchoPass и поднесите телефон к микрофону
            ноутбука. Мы декодируем токен локально и подтвердим вход без пароля или SMS.
          </p>
          <div className="app__actions">
            <button
              type="button"
              className="app__button"
              onClick={handleStart}
              disabled={isBusy || flowStatus === 'verifying'}
            >
              {isBusy ? 'Запрос выполняется...' : 'Войти по звуку'}
            </button>
            <button type="button" className="app__button app__secondary">
              Альтернативный вход (e-mail)
            </button>
          </div>

          <div className="app__info">
            <strong>Инструкции:</strong>
            <span>1. Нажмите кнопку «Войти по звуку».</span>
            <span>2. В приложении нажмите «Проиграть токен» и наведите динамик на микрофон.</span>
            <span>3. Дождитесь подтверждения входа, не отходите от ноутбука.</span>
            <MicLevelMeter level={level} active={decoder.status === 'listening'} />
          </div>

          {activeChallenge && (
            <div className="app__challenge">
              <span className="app__challenge-label">Активный challenge</span>
              <span className="app__challenge-value">
                {formatChallengeDecimal(activeChallenge.challenge_id)}
              </span>
              <span className="app__challenge-sub">
                HEX: {formatChallengeHex(activeChallenge.challenge_id)} · TTL: {countdown}s
              </span>
            </div>
          )}

          <div className="app__status">
            <strong>Статус</strong>
            <span>{statusMessage}</span>
            <span>Движок GGWave: {decoder.ready ? 'готов' : 'загружается'}.</span>
            <span>Режим прослушивания: {decodeStatusLabel(decoder.status)}.</span>
          </div>

          {lastTokenBase64 && flowStatus !== 'success' && (
            <div className="app__status">
              <strong>Последний токен (base64)</strong>
              <span style={{ wordBreak: 'break-all' }}>{lastTokenBase64}</span>
            </div>
          )}

          {error && <div className="app__error">{error}</div>}

          {session?.ok && (
            <p className="app__session">
              Активная сессия: <strong>{session.userId}</strong>
            </p>
          )}
      </section>

      <aside className="app__right">
          <h2 style={{ margin: 0 }}>Подсказки по качеству звука</h2>
          <ul className="app__checklist">
            <li>
              <svg width="20" height="20" viewBox="0 0 20 20" fill="none" xmlns="http://www.w3.org/2000/svg">
                <path
                  d="M8.333 13.232 5.606 10.51l-1.178 1.179 3.905 3.905 7.5-7.5-1.178-1.178-6.322 6.316z"
                  fill="currentColor"
                />
              </svg>
              Держите телефон на расстоянии 10–30 см от микрофона, избегайте перекрытия.
            </li>
            <li>
              <svg width="20" height="20" viewBox="0 0 20 20" fill="none" xmlns="http://www.w3.org/2000/svg">
                <path
                  d="M8.333 13.232 5.606 10.51l-1.178 1.179 3.905 3.905 7.5-7.5-1.178-1.178-6.322 6.316z"
                  fill="currentColor"
                />
              </svg>
              Выключите музыку и посторонние шумы, закройте окна или переместитесь в тихое место.
            </li>
            <li>
              <svg width="20" height="20" viewBox="0 0 20 20" fill="none" xmlns="http://www.w3.org/2000/svg">
                <path
                  d="M8.333 13.232 5.606 10.51l-1.178 1.179 3.905 3.905 7.5-7.5-1.178-1.178-6.322 6.316z"
                  fill="currentColor"
                />
              </svg>
              Если вход не удался, повторите challenge и токен не чаще одного раза подряд.
            </li>
            <li>
              <svg width="20" height="20" viewBox="0 0 20 20" fill="none" xmlns="http://www.w3.org/2000/svg">
                <path
                  d="M8.333 13.232 5.606 10.51l-1.178 1.179 3.905 3.905 7.5-7.5-1.178-1.178-6.322 6.316z"
                  fill="currentColor"
                />
              </svg>
              Альтернативный вход (по e-mail) доступен, если микрофон заблокирован.
            </li>
          </ul>
      </aside>
    </main>
  );

  return (
    <div className="app">
      <header className="app__header">
        <div className="app__brand">EchoPass · Acoustic Authentication Token</div>
        <nav className="app__nav">
          <button
            type="button"
            className={`app__nav-button ${activeTab === 'login' ? 'app__nav-button--active' : ''}`}
            onClick={() => setActiveTab('login')}
          >
            Вход по звуку
          </button>
          <button
            type="button"
            className={`app__nav-button ${activeTab === 'admin' ? 'app__nav-button--active' : ''}`}
            onClick={() => setActiveTab('admin')}
          >
            Админ-панель
          </button>
        </nav>
      </header>

      {activeTab === 'login' ? renderLogin() : (
        <main className="app__card app__card--admin">
          <AdminPanel />
        </main>
      )}
    </div>
  );
}

export default App;
