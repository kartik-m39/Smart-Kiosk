"use client";
import { useEffect, useState, useRef } from "react";

export default function HealthScanPage() {
  const [bpm, setBpm] = useState<number | null>(null);
  const [spo2, setSpo2] = useState<number | null>(null);
  const [loading, setLoading] = useState(true);
  const [visible, setVisible] = useState(false);
  const [fadeOut, setFadeOut] = useState(false);
  const [timeLeft, setTimeLeft] = useState(20);
  const timerRef = useRef<ReturnType<typeof setInterval> | null>(null);
  const hideTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  useEffect(() => {
    const ws = new WebSocket("wss://kwf1lz9w-3000.inc1.devtunnels.ms/");

    ws.onmessage = (event) => {
      const data = JSON.parse(event.data);
      if (data.type !== "health") return;

      setBpm(data.bpm);
      setSpo2(data.spo2);
      setLoading(false);
      setVisible(true);
      setFadeOut(false);
      setTimeLeft(20);

      // Clear any existing timers
      if (timerRef.current) clearInterval(timerRef.current);
      if (hideTimerRef.current) clearTimeout(hideTimerRef.current);

      // Countdown
      timerRef.current = setInterval(() => {
        setTimeLeft((prev) => {
          if (prev <= 1) {
            clearInterval(timerRef.current!);
            return 0;
          }
          return prev - 1;
        });
      }, 1000);

      // Fade out at 17s, hide at 20s
      hideTimerRef.current = setTimeout(() => {
        setFadeOut(true);
        setTimeout(() => {
          setVisible(false);
          setLoading(true);
        }, 3000);
      }, 17000);
    };

    return () => {
      ws.close();
      if (timerRef.current) clearInterval(timerRef.current);
      if (hideTimerRef.current) clearTimeout(hideTimerRef.current);
    };
  }, []);

  const bpmStatus =
    bpm !== null
      ? bpm < 60
        ? { label: "Low", color: "#f59e0b" }
        : bpm <= 100
        ? { label: "Normal", color: "#22c55e" }
        : { label: "Elevated", color: "#ef4444" }
      : null;

  const spo2Status =
    spo2 !== null
      ? spo2 >= 95
        ? { label: "Healthy", color: "#22c55e" }
        : spo2 >= 90
        ? { label: "Borderline", color: "#f59e0b" }
        : { label: "Low", color: "#ef4444" }
      : null;

  return (
    <>
      <style>{`
        @import url('https://fonts.googleapis.com/css2?family=Playfair+Display:wght@400;600;700&family=DM+Sans:wght@300;400;500&display=swap');

        :root {
          --green-deep: #1a3d2b;
          --green-mid: #2d6a4f;
          --green-light: #52b788;
          --green-pale: #b7e4c7;
          --green-mist: #d8f3dc;
          --cream: #f8faf6;
          --gold: #a67c52;
          --text-dark: #1a2e1e;
          --text-mid: #3a5c42;
          --text-soft: #6b8f71;
        }

        * { box-sizing: border-box; margin: 0; padding: 0; }

        body {
          background: var(--cream);
          min-height: 100vh;
        }

        .page {
          min-height: 100vh;
          display: flex;
          align-items: center;
          justify-content: center;
          background:
            radial-gradient(ellipse at 20% 10%, rgba(82,183,136,0.18) 0%, transparent 50%),
            radial-gradient(ellipse at 80% 90%, rgba(45,106,79,0.15) 0%, transparent 50%),
            radial-gradient(ellipse at 60% 30%, rgba(183,228,199,0.12) 0%, transparent 40%),
            var(--cream);
          font-family: 'DM Sans', sans-serif;
          position: relative;
          overflow: hidden;
          padding: 24px;
        }

        /* Floating leaf decorations */
        .leaf {
          position: absolute;
          opacity: 0.07;
          pointer-events: none;
        }
        .leaf-1 {
          top: -40px; left: -40px; width: 280px;
          animation: floatLeaf 8s ease-in-out infinite;
        }
        .leaf-2 {
          bottom: -60px; right: -60px; width: 320px;
          animation: floatLeaf 10s ease-in-out infinite reverse;
        }
        .leaf-3 {
          top: 40%; left: -80px; width: 200px;
          animation: floatLeaf 12s ease-in-out infinite 2s;
          opacity: 0.05;
        }

        @keyframes floatLeaf {
          0%, 100% { transform: rotate(0deg) translateY(0px); }
          50% { transform: rotate(8deg) translateY(-20px); }
        }

        /* Dot grid texture */
        .page::before {
          content: '';
          position: absolute;
          inset: 0;
          background-image: radial-gradient(circle, rgba(45,106,79,0.08) 1px, transparent 1px);
          background-size: 32px 32px;
          pointer-events: none;
        }

        .card {
          background: rgba(255,255,255,0.88);
          backdrop-filter: blur(16px);
          border: 1px solid rgba(82,183,136,0.25);
          border-radius: 28px;
          padding: 48px 44px 40px;
          width: 100%;
          max-width: 480px;
          box-shadow:
            0 4px 6px rgba(26,61,43,0.04),
            0 20px 60px rgba(45,106,79,0.12),
            0 0 0 1px rgba(255,255,255,0.6) inset;
          position: relative;
          z-index: 1;
          animation: cardIn 0.7s cubic-bezier(0.22,1,0.36,1) both;
        }

        @keyframes cardIn {
          from { opacity: 0; transform: translateY(30px) scale(0.97); }
          to   { opacity: 1; transform: translateY(0) scale(1); }
        }

        /* Top accent bar */
        .card::before {
          content: '';
          position: absolute;
          top: 0; left: 28px; right: 28px; height: 3px;
          background: linear-gradient(90deg, var(--green-light), var(--green-mid), var(--green-light));
          border-radius: 0 0 4px 4px;
        }

        .header {
          text-align: center;
          margin-bottom: 36px;
        }

        .icon-wrap {
          width: 64px; height: 64px;
          background: linear-gradient(135deg, var(--green-mist), var(--green-pale));
          border-radius: 20px;
          display: flex; align-items: center; justify-content: center;
          margin: 0 auto 16px;
          border: 1px solid rgba(82,183,136,0.3);
          box-shadow: 0 4px 16px rgba(45,106,79,0.15);
        }

        .icon-wrap svg { width: 28px; height: 28px; }

        .title {
          font-family: 'Playfair Display', serif;
          font-size: 26px;
          font-weight: 700;
          color: var(--green-deep);
          letter-spacing: -0.3px;
          line-height: 1.2;
        }

        .subtitle {
          font-size: 13.5px;
          color: var(--text-soft);
          margin-top: 6px;
          font-weight: 400;
          letter-spacing: 0.3px;
        }

        /* Loading state */
        .loading-wrap {
          text-align: center;
          padding: 16px 0 8px;
        }

        .pulse-ring {
          width: 72px; height: 72px;
          margin: 0 auto 20px;
          position: relative;
        }

        .pulse-ring::before,
        .pulse-ring::after {
          content: '';
          position: absolute;
          inset: 0;
          border-radius: 50%;
          border: 2.5px solid var(--green-light);
          animation: pulseRing 2s ease-out infinite;
        }
        .pulse-ring::after { animation-delay: 0.7s; }

        .pulse-core {
          position: absolute;
          inset: 14px;
          background: linear-gradient(135deg, var(--green-light), var(--green-mid));
          border-radius: 50%;
          animation: pulseBeat 1.4s ease-in-out infinite;
        }

        @keyframes pulseRing {
          0% { transform: scale(0.8); opacity: 1; }
          100% { transform: scale(1.8); opacity: 0; }
        }

        @keyframes pulseBeat {
          0%, 100% { transform: scale(1); }
          50% { transform: scale(0.88); }
        }

        .loading-text {
          font-size: 15px;
          color: var(--text-mid);
          font-weight: 500;
        }

        .loading-dots span {
          animation: dotBlink 1.4s infinite;
          opacity: 0;
        }
        .loading-dots span:nth-child(2) { animation-delay: 0.2s; }
        .loading-dots span:nth-child(3) { animation-delay: 0.4s; }

        @keyframes dotBlink {
          0%, 80%, 100% { opacity: 0; }
          40% { opacity: 1; }
        }

        /* Results */
        .results {
          transition: opacity 1.5s ease, transform 1.5s ease;
        }
        .results.fade-out {
          opacity: 0;
          transform: translateY(12px);
        }

        .vitals-grid {
          display: grid;
          grid-template-columns: 1fr 1fr;
          gap: 14px;
          margin-bottom: 20px;
        }

        .vital-card {
          background: linear-gradient(145deg, var(--green-mist), rgba(255,255,255,0.9));
          border: 1px solid rgba(82,183,136,0.22);
          border-radius: 18px;
          padding: 20px 16px 18px;
          text-align: center;
          position: relative;
          overflow: hidden;
          animation: vitalIn 0.6s cubic-bezier(0.22,1,0.36,1) both;
          box-shadow: 0 2px 12px rgba(45,106,79,0.08);
        }

        .vital-card:nth-child(1) { animation-delay: 0.1s; }
        .vital-card:nth-child(2) { animation-delay: 0.2s; }

        @keyframes vitalIn {
          from { opacity: 0; transform: translateY(16px) scale(0.95); }
          to   { opacity: 1; transform: translateY(0) scale(1); }
        }

        .vital-card::after {
          content: '';
          position: absolute;
          bottom: 0; left: 0; right: 0; height: 3px;
          background: var(--bar-color, var(--green-light));
          border-radius: 0 0 18px 18px;
          opacity: 0.7;
        }

        .vital-emoji {
          font-size: 26px;
          display: block;
          margin-bottom: 8px;
          animation: heartbeat 1.4s ease-in-out infinite;
          line-height: 1;
        }

        .vital-card:nth-child(2) .vital-emoji {
          animation: breathe 3s ease-in-out infinite;
        }

        @keyframes heartbeat {
          0%, 100% { transform: scale(1); }
          14% { transform: scale(1.2); }
          28% { transform: scale(1); }
          42% { transform: scale(1.15); }
          70% { transform: scale(1); }
        }

        @keyframes breathe {
          0%, 100% { transform: scale(1); }
          50% { transform: scale(1.12); }
        }

        .vital-value {
          font-family: 'Playfair Display', serif;
          font-size: 38px;
          font-weight: 700;
          color: var(--green-deep);
          line-height: 1;
          letter-spacing: -1px;
        }

        .vital-unit {
          font-family: 'DM Sans', sans-serif;
          font-size: 13px;
          font-weight: 500;
          color: var(--text-soft);
          margin-top: 2px;
          text-transform: uppercase;
          letter-spacing: 0.8px;
        }

        .vital-status {
          display: inline-block;
          margin-top: 8px;
          font-size: 11px;
          font-weight: 600;
          letter-spacing: 0.6px;
          text-transform: uppercase;
          padding: 3px 9px;
          border-radius: 20px;
          background: rgba(255,255,255,0.7);
          border: 1px solid rgba(82,183,136,0.2);
        }

        /* Timer bar */
        .timer-wrap {
          margin-top: 20px;
        }

        .timer-label {
          display: flex;
          justify-content: space-between;
          align-items: center;
          margin-bottom: 7px;
        }

        .timer-text {
          font-size: 12px;
          color: var(--text-soft);
          font-weight: 500;
          letter-spacing: 0.3px;
        }

        .timer-count {
          font-size: 12px;
          font-weight: 600;
          color: var(--green-mid);
          font-variant-numeric: tabular-nums;
        }

        .timer-track {
          height: 5px;
          background: rgba(82,183,136,0.15);
          border-radius: 10px;
          overflow: hidden;
        }

        .timer-fill {
          height: 100%;
          background: linear-gradient(90deg, var(--green-light), var(--green-mid));
          border-radius: 10px;
          transition: width 1s linear;
        }

        /* Footer note */
        .scan-note {
          text-align: center;
          margin-top: 22px;
          font-size: 12px;
          color: var(--text-soft);
          display: flex;
          align-items: center;
          justify-content: center;
          gap: 6px;
        }

        .scan-note::before,
        .scan-note::after {
          content: '';
          flex: 1;
          height: 1px;
          background: linear-gradient(90deg, transparent, rgba(82,183,136,0.25), transparent);
        }
      `}</style>

      <div className="page">
        {/* Leaf SVGs */}
        <svg className="leaf leaf-1" viewBox="0 0 200 200" xmlns="http://www.w3.org/2000/svg">
          <path d="M10,100 Q50,10 150,30 Q180,80 100,160 Q40,170 10,100Z" fill="#2d6a4f"/>
          <line x1="10" y1="100" x2="130" y2="90" stroke="#52b788" strokeWidth="2"/>
        </svg>
        <svg className="leaf leaf-2" viewBox="0 0 200 200" xmlns="http://www.w3.org/2000/svg">
          <path d="M20,150 Q60,20 170,40 Q190,110 120,170 Q60,190 20,150Z" fill="#1a3d2b"/>
        </svg>
        <svg className="leaf leaf-3" viewBox="0 0 150 150" xmlns="http://www.w3.org/2000/svg">
          <path d="M10,80 Q40,10 120,20 Q140,70 80,130 Q30,140 10,80Z" fill="#52b788"/>
        </svg>

        <div className="card">
          <div className="header">
            <div className="icon-wrap">
              <svg viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
                <path d="M12 21C12 21 3 14.5 3 8.5C3 5.46 5.46 3 8.5 3C10.24 3 11.91 3.81 13 5.08C14.09 3.81 15.76 3 17.5 3C20.54 3 23 5.46 23 8.5C23 14.5 14 21 12 21Z" fill="#2d6a4f" opacity="0.2"/>
                <path d="M2 12H5L7 8L10 16L13 10L15 13H22" stroke="#2d6a4f" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round"/>
              </svg>
            </div>
            <h1 className="title">Health Scan</h1>
            <p className="subtitle">Real-time vitals monitoring</p>
          </div>

          {loading ? (
            <div className="loading-wrap">
              <div className="pulse-ring">
                <div className="pulse-core" />
              </div>
              <p className="loading-text">
                Analyzing vitals<span className="loading-dots"><span>.</span><span>.</span><span>.</span></span>
              </p>
            </div>
          ) : visible ? (
            <div className={`results${fadeOut ? " fade-out" : ""}`}>
              <div className="vitals-grid">
                {/* Heart Rate */}
                <div
                  className="vital-card"
                  style={{ "--bar-color": bpmStatus?.color } as React.CSSProperties}
                >
                  <span className="vital-emoji">❤️</span>
                  <div className="vital-value">{bpm}</div>
                  <div className="vital-unit">BPM</div>
                  {bpmStatus && (
                    <span className="vital-status" style={{ color: bpmStatus.color }}>
                      {bpmStatus.label}
                    </span>
                  )}
                </div>

                {/* SpO₂ */}
                <div
                  className="vital-card"
                  style={{ "--bar-color": spo2Status?.color } as React.CSSProperties}
                >
                  <span className="vital-emoji">🫁</span>
                  <div className="vital-value">{spo2}</div>
                  <div className="vital-unit">SpO₂ %</div>
                  {spo2Status && (
                    <span className="vital-status" style={{ color: spo2Status.color }}>
                      {spo2Status.label}
                    </span>
                  )}
                </div>
              </div>

              {/* Countdown timer */}
              <div className="timer-wrap">
                <div className="timer-label">
                  <span className="timer-text">Results visible for</span>
                  <span className="timer-count">{timeLeft}s</span>
                </div>
                <div className="timer-track">
                  <div
                    className="timer-fill"
                    style={{ width: `${(timeLeft / 20) * 100}%` }}
                  />
                </div>
              </div>

              <p className="scan-note">Scan complete</p>
            </div>
          ) : null}
        </div>
      </div>
    </>
  );
}