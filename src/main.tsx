import {render} from 'preact';
import {App} from './app';
import {loadConfig} from './lib/storage';
import {applyTokens} from './theme/tokens';
import './theme/global.css';

applyTokens(loadConfig().theme);       // theme first, so there is no flash of the wrong colours
render(<App />, document.getElementById('root')!);
