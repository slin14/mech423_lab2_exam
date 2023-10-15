volatile unsigned int  byteState = 0;
switch(byteState) {
	case 0:
		if (...) byteState = 1;
		break;
	case 1:
		byteState = 2;
		break;
	case 2:
		byteState = 3;
		break;
	case 3:
		byteState = 4;
		break;
	case 4:
		byteState = 0;
		break;
	default:
		break;
}

