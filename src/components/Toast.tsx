import {useRef} from 'preact/hooks';
import {useSnapWidths} from '../hooks/useSnapWidths';

export interface ToastMessage {
	text: string;
	kind?: 'info' | 'error';
}

export function Toast ({message}: {message: ToastMessage | null}) {
	const ref = useRef<HTMLDivElement>(null);
	useSnapWidths(ref, '.toast', true);         // centred: an even width keeps its text on whole pixels
	if (!message) return null;
	return <div class={`toast${message.kind === 'error' ? ' toast--error' : ''}`} ref={ref}>{message.text}</div>;
}
