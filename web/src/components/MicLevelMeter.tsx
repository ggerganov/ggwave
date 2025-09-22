import './MicLevelMeter.css';

interface MicLevelMeterProps {
  level: number;
  active: boolean;
}

export const MicLevelMeter = ({ level, active }: MicLevelMeterProps) => {
  const percentage = Math.min(100, Math.round(level * 100));
  return (
    <div className={`mic-meter ${active ? 'mic-meter--active' : ''}`} aria-live="polite">
      <div className="mic-meter__bar" style={{ width: `${percentage}%` }} />
      <span className="mic-meter__label">Уровень сигнала: {percentage}%</span>
    </div>
  );
};
