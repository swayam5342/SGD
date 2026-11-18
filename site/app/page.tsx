import Nav from '@/components/Nav';
import Hero from '@/components/Hero';
import Jobs from '@/components/Jobs';
import UseCases from '@/components/UseCases';
import Features from '@/components/Features';
import Calibrate, { Statement } from '@/components/Calibrate';
import Questions from '@/components/Questions';
import Closing from '@/components/Closing';

/*
  type: software (developer tool)
  deal: hero triptych, nav nav-mega, close close-type, footer footer-statement, rhythm space, buttons solid,
        mark wordmark, palette green-black-bone (a near-black terminal ground with a muted green status light:
        the tool lives in a terminal), pairing funnel, ground sand, effect gradient-mesh in the one
        [data-visual] frame (#calibrate), card ground painterly-landscape
  moments: liquid-type on #measured (the statement that costs come from the machine)
*/
export default function Home() {
  return (
    <>
      <Nav />
      <main id="top">
        <Hero />
        <UseCases />
        <Jobs />
        <Features />
        <Statement />
        <Calibrate />
        <Questions />
      </main>
      <Closing />
    </>
  );
}
